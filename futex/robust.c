/*
 * Copyright 2009 IBM Corp.
 * Author: Matt Helsley <matthltc@us.ibm.com>
 *
 * Test simple (non-pi) robust futexes across checkpoint/restart.
 * See Documentation/robust-futexes.txt (and other futex docs in that
 * kernel source directory)
 *
 * Robust futex lists are shared with the kernel. They are per-thread lists
 * of acquired futexes. When a thread/task exits the kernel walks this list,
 * WAKE'ing one waiter for each futex it still holds. This ensures that tasks
 * which die while holding a futex do not necessarily prevent other tasks
 * from recovering.
 *
 * When the futex owner (see below) dies the FUTEX_OWNER_DIED bit is set
 * (0x40000000)
 *
 * Waiters must set the FUTEX_WAITERS bit (0x80000000) and use the remaining
 * bits for the TID of the task that "owns" the futex.
 *
 * See README for information on interpretting the log output.
 */

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <asm/mman.h> /* for PROT_SEM */
#include <linux/futex.h>

#include "libfutex/libfutex.h"
#include "libfutex/atomic.h"

#include "libcrtest/libcrtest.h"

#define LOG_FILE	"log.robust"
FILE *logfp = NULL;
atomic_t log_lock = { 0 };

/* number of child processes to WAIT on futex. Must be >= 2. */
#define N 3

/* From the Linux kernel */
#ifndef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif

int pass = 0;
int fail = 0;

/* Children send ready-status bytes to the parent via this pipe. */
#define CHILD_READY 0
#define CHILD_ERROR 255
int children_ready[2];

struct futex {
	atomic_t tid;
	struct robust_list rlist;
};

struct futex *test_futex;

struct robust_list_head rlist = {
	.list = {
		/*
		 * Circular singly-linked list with each next field pointing to
		 * the next field of the next list element.
		 */
		.next = &rlist.list,
	},

	/*
	 * Offset of the futex word relative to the next entry of its
	 * robust_list head.
	 */
	.futex_offset = offsetof(struct futex, tid) - offsetof(struct futex, rlist),
	/*
	 * Set list_op_pending before acquiring the futex and
	 * clears it once the futex has been added to rlist.
	 */
	.list_op_pending = NULL
};

void add_rfutex(struct futex *rf)
{
	log("INFO", "adding test_futex\n");
	rf->rlist.next = rlist.list.next;
	rlist.list.next  = &rf->rlist;
	rlist.list_op_pending = NULL; /* ARCH TODO make assign atomic */
}

void acquire_rfutex(struct futex *rf, pid_t tid)
{
	struct timespec timeout = {
		.tv_sec = 5,
		.tv_nsec = 0
	};
	int oldval, newval, val = 0;

	rlist.list_op_pending = &rf->rlist; /* ARCH TODO make sure this assignment is atomic */

	oldval = atomic_read(&rf->tid);
	tid = tid & FUTEX_TID_MASK;
	do {
		val = atomic_cmpxchg(&rf->tid, 0, tid);
		if (val == 0) {
			log("FAIL", "did not see contended futex\n");
			fail++;
			break;
		}

		/*
		 * else we're contended -- this is the path we always take
		 * the first time through this loop in this test program.
		 *
		 * Set the WAITERS bit to indicate that we need to be woken.
		 */
		val = __sync_or_and_fetch(&rf->tid.counter, FUTEX_WAITERS);
		log("INFO", "futex(FUTEX_WAIT, %x)\n", val);
		if (futex(&rf->tid.counter, FUTEX_WAIT, val,
			  &timeout, NULL, 0) == 0)
			break;
		log("INFO", "futex returned with errno %d (%s).\n", errno, strerror(errno));
		switch(errno) {
			case ERESTART:
				log("WARN", "ERESTART while sleeping on futex\n");
				continue;
			case EAGAIN:
				log("WARN", "EAGAIN while sleeping on futex\n");
				newval = atomic_read(&rf->tid);
				if (newval != oldval) {
					int ret = creat("TBROK", 0755);
					if (ret == -1)
						fail++;
					return;
				}
				continue;
			case EINTR:
				log("WARN", "EINTR while sleeping on futex\n");
				continue;
			case ETIMEDOUT:
				log("FAIL", "ETIMEDOUT while sleeping on futex.\n");
				fail++;
				return;
			case EACCES:
				log("FAIL", "FUTEX_WAIT EACCES - no read access to futex memory\n");
				fail++;
				return;
			case EFAULT:
				log("FAIL", "FUTEX_WAIT EFAULT - bad timeout timespec address or futex address\n");
				fail++;
				return;
			case EINVAL:
				log("FAIL", "FUTEX_WAIT EINVAL - undefined futex operation\n");
				fail++;
				return;
			case ENOSYS:
				log("FAIL", "FUTEX_WAIT ENOSYS - undefined futex operation\n");
				fail++;
				return;
			default:
				log_error("FUTEX_WAIT unexpected error (missing from man page)");
				fail++;
				return;
		}
	} while(1);

	log("INFO", "holding futex.\n");

	val = atomic_read(&rf->tid);
	if (val & FUTEX_OWNER_DIED)
		/* could change INFO to PASS if we could know that we're not
		   the first child to acquire the futex */
		log("INFO", "previous owner died before got futex.\n");

	/*
	 * Recovering the futex so it's OK to clear FUTEX_OWNER_DIED
	 * but we must preserve the FUTEX_WAITERS bit.
	 */
	atomic_set(&rf->tid, tid|(val & FUTEX_WAITERS));
	add_rfutex(rf);
}

int release_rfutex(struct futex *rf, pid_t tid, int i)
{
	int val;

	val = atomic_cmpxchg(&rf->tid, tid, 0);
	if (val == tid) {
		log("FAIL", "No waiters on futex.\n");
		fail++;
		return -1;
	}

	if (futex(&rf->tid.counter, FUTEX_WAKE, 1, NULL, NULL, 0) != 1) {
		log_error("futex(FUTEX_WAKE)");
		log("FAIL", "%d (see above for error string)\n", errno);
		fail++;
		return -1;
	}

	/*
	 * Technically, we're supposed to remove it from the robust list,
	 * but only the parent is supposed to release the futex in this
	 * test. Since it starts holding the futex and is "guaranteed" to
	 * release it, we don't bother with adding or removing it
	 * from the robust list.
	 */
	return 0;
}

/* Make sure the robust list is set correctly */
int check_rlist(int i)
{
	struct robust_list_head *fetched_rlist = NULL;
	size_t fetched_rlist_size = 0;
	int rc;

	rc = get_robust_list(0, &fetched_rlist, &fetched_rlist_size);
	if (rc < 0) {
		log("FAIL", "getting robust list %d failed.\n", i);
		fail++;
		return -1;
	}

	if ((fetched_rlist == &rlist) &&
	    (fetched_rlist_size == sizeof(rlist))) {
		pass++;
		return 0;
	} else  {
		log("FAIL", "checking robust list %d: got: (%p size: %zd) expected: (%p size: %zd)\n", i,
		    fetched_rlist, fetched_rlist_size,
		    &rlist, sizeof(rlist));
		fail++;
		return -1;
	}
}

void send_parent_status(int *fd, char status)
{
	while (write(*fd, &status, sizeof(status)) != 1) {}
	close(*fd);
	*fd = -1;
}

int kid(int i)
{
	if (set_robust_list(&rlist, sizeof(rlist)) < 0) {
		log_error("set_robust_list");
		send_parent_status(&children_ready[1], CHILD_ERROR);
		fail++;
		goto do_exit;
	}
	if (check_rlist(i) != 0) {
		send_parent_status(&children_ready[1], CHILD_ERROR);
		fail++;
		goto do_exit;
	}

	log("INFO", "signaling ready for checkpointing\n");
	set_checkpoint_ready();
	while (!test_checkpoint_done()) { sleep(1); }

	if (check_rlist(i) != 0) {
		send_parent_status(&children_ready[1], CHILD_ERROR);
		fail++;
		goto do_exit;
	}

	send_parent_status(&children_ready[1], CHILD_READY);
	acquire_rfutex(test_futex, gettid());
	pass++;

	/*
	 * Now exit instead of releasing the futex. This should cause
	 * the kernel to wake the next waiter with FUTEX_OWNER_DIED.
	 */
do_exit:
	log("INFO", "exiting\n");
	if (pass && !fail)
		exit(EXIT_SUCCESS);
	exit(EXIT_FAILURE);
}

void dump (const char *prefix)
{
	log("INFO", "%s futex: %d\n", prefix, atomic_read(&test_futex->tid));
}

void sig_dump(int signum)
{
	dump("Ctrl-C Interrupt sample:");
}

int main(int argc, char **argv)
{
	pid_t kids[N];
	int i, excode = EXIT_FAILURE;

	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("FAIL: logfile");/* perror() since logfp unopened */
		exit(excode);
	}
	/* redirect stdout and stderr to the log file */
	if ((dup2(fileno(logfp), 1) != 1) ||
	    (dup2(fileno(logfp), 2) != 2)) {
		log_error("dup2() logfp to stdout and stderr");
		goto exit_logs;
	}

	if (!move_to_cgroup("freezer", "1", getpid())) {
		log_error("move_to_cgroup");
		goto exit_logs;
	}

	/*
	 * Create the pipes that children use to tell us when they get to
	 * specific points. We use this instead of racier sleeps.
	 */
	if (pipe(children_ready) == -1) {
		log_error("pipe(children_ready)");
		goto exit_logs;
	}

	/*
	 * Create the futex. We can't use alloc_futex_mem() since we need
	 * MAP_SHARED.
	 */
	test_futex = mmap(NULL, sizeof(*test_futex),
			  PROT_READ|PROT_WRITE|PROT_SEM,
			  MAP_ANONYMOUS|MAP_SHARED, -1, 0);
	if (test_futex == MAP_FAILED) {
		log_error("mmap shared futex");
		goto exit_pipe;
	}

	/* Should already be zero but let's be clear about that. */
	atomic_set(&test_futex->tid, 0);
	test_futex->rlist.next = &test_futex->rlist;

	if (set_robust_list(&rlist, sizeof(rlist))) {
		log_error("set_robust_list");
		goto exit_pipe;
	}
	check_rlist(0);

	/* Give the futex to the parent initially */
	atomic_set(&test_futex->tid, gettid());
	signal(SIGINT, sig_dump);
	for (i = 0; i < N; i++) {
		/*
		 * Each thread starts with it's own empty robust list.
		 * set_robust_list() must be called from the thread before
		 * this list can record held futexes.
		*/
		kids[i] = fork();
		if (kids[i] < 0)
			break;
		else if (kids[i] == 0) {
			close(children_ready[0]);
			kid(i + 1);
		}
	}

	if (i < N) {
		log_error("N x FUTEX_WAIT");
		fail++;
		log("INFO", "killing %d child tasks.\n", i);
		for (; --i > -1;)
			kill(kids[i], SIGTERM);
		goto exit_pipe;
	}

	do {
		char status;

		if (read(children_ready[0], &status, 1) != 1)
			continue;
		if (status == CHILD_READY)
			pass++;
		else
			fail++;
	} while (pass + fail < N);

	/* Now that all the children are waiting on the futex, wake one. */
	log("INFO", "Parent waking one child\n");
	release_rfutex(test_futex, gettid(), 0);
	log("INFO", "Parent waiting for children\n");
	do_wait(N); /* N if we're not using CLONE_THREAD, 1 otherwise */
	log("INFO", "Parent exiting.\n");
	if (pass && !fail)
		excode = EXIT_SUCCESS;
exit_pipe:
	close(children_ready[0]);
	close(children_ready[1]);
exit_logs:
	fclose(logfp);
	exit(excode);
}
