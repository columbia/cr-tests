/*
 * Copyright 2009 IBM Corp.
 * Author: Matt Helsley <matthltc@us.ibm.com>
 *
 * Test priority inheritance of futexes along with checkpoint/restart.
 *
 * This test starts multiple child processes each with succesively
 * higher priority. The lowest priority child grabs a pi futex while
 * all of the higher priority children wait on a plain futex. Once
 * it has the pi futex the lowest priority child wakes up the other
 * children so that they will contend for the pi futex. The lowest
 * priority child can then watch its priority rise to that of the
 * highest priority child because it holds the futex.
 *
 * Then the lowest priority child releases the futex and thus wakes
 * the highest priority child. Each of the contended children is
 * subsequently woken in priority order -- so it does not inherit
 * elevated priority -- until the last child releases the futex.
 *
 * NOTES:
 *
 * See README for information on interpretting the log output.
 *
 * Since this test sets realtime priorities, the user running the tests
 * needs permission to set realtime priorities. To see if the user has permission:
 * $ ulimit -r
 * N
 */
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <asm/mman.h> /* for PROT_SEM */
#include <string.h>
#include <linux/futex.h>

#include "libfutex/libfutex.h"
#include "libfutex/atomic.h"
#include "libcrtest/libcrtest.h"


extern int clone(int (*fn)(void *), void *child_stack, int flags, void *arg, ...);

/*
 * The globals are set up from the main thread and then left untouched
 * by the children.
 */
#define LOG_FILE	"log.pi"
FILE *logfp = NULL;
atomic_t log_lock = { 0 };

/*
 * Number of child processes to WAIT on futex -- must be less than number
 * of priority levels available.
 */
unsigned int N = 3;

const int clone_flags = CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_VM|CLONE_SYSVSEM|SIGCHLD; //|CLONE_THREAD|CLONE_PARENT;
int prio_min, prio_max, sched_policy = SCHED_RR;

/* Each child pid is recorded in kids[] */
pid_t *kids;

/* These record the progress of the children so we can dump it for checkpoint */
atomic_t dumb_barrier[1] = { {0} };

/* In order to create the priority inversion high priority threads sleep here */
atomic_t *waitq;

/* The pi futex itself */
atomic_t *pi_futex;

/*
 * Normal priority functions deal with static priority -- priority that
 * doesn't change unless userspace asks nicely. The nice, rtpriority,
 * and normal_prio of tasks are these kinds of priorities.
 *
 * The priority is only modified if getpriority succeeded and we return 0.
 * Otherwise we return -1, put an error in errno, and do not modify the
 * parameter.
 */
int get_my_static_priority(int *prio)
{
	struct sched_param param;

	if (sched_getparam(gettid(), &param) == 0) {
		*prio = param.sched_priority;
		return 0;
	}
	return -1;
}

int set_my_static_priority(int prio)
{
	struct sched_param param;

	param.sched_priority = prio;
	return sched_setparam(gettid(), &param);
}

/*
 * We need to determine the instantaneous priority of a thread. So
 * we look in /proc. This isn't racy because we're cooperating with
 * the threads -- they should be waiting on the pi futex so their
 * dynamic priorities shouldn't change.
 *
 * Fetch the dynamic priority from the 18th field of
 * /proc/<tgid>/task/<tid>/stat and transform it from a kernel priority
 * number to realtime priority number suitable for comparison with
 * get|set_my_static_priority() above.
 */
int get_dynamic_priority(pid_t tid, int *dpriority)
{
	char buffer[4096];
	int fd;
	int retval = -1;
	pid_t tgid;

	if (clone_flags & CLONE_THREAD)
		tgid = getpid();
	else
		tgid = tid;

	*dpriority = INT_MAX;
	snprintf(buffer, sizeof(buffer),
		 "/proc/%d/task/%d/stat", tgid, tid);
	fd = open(buffer, O_RDONLY);
	if (fd < 0)
		goto out;
	if (read(fd, buffer, sizeof(buffer)) < 0)
		goto out;
	close(fd);
	buffer[sizeof(buffer) - 1] = '\0';

	if (sscanf(buffer, " %*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %d %*d %*d 0", dpriority) != 1)
		goto out;
	retval = 0;

	/* Transform the priority */
	*dpriority = -1*(*dpriority + 1);
out:
	return retval;
}

void dump_dynamic_priorities(void)
{
	/*
	 * Since multiple calls to log() are not "atomic" we accumulate the
	 * output in a temporary buffer then pass it all to log()
	 */
	char buffer[4096];
	char *pos;
	int prio;
	unsigned int i;

	pos = buffer;
	for (i = 0; i < N; i++) {
		if (get_dynamic_priority(kids[i], &prio) != 0) {
			pos += snprintf(pos, sizeof(buffer) - (pos - buffer),
				 " %d: warning = \"%s\"",
				 kids[i], strerror(errno));
		} else
			pos += snprintf(pos, sizeof(buffer) - (pos - buffer),
				 " %d: %d", kids[i], prio);
	}
	log("INFO", "dynamic priorities: %s\n", buffer);
}

/*
 * All the uses of the futex() syscall in this test are wrapped by
 * functions with nice names, finite retry loops, and verbose error
 * reporting.
 */

int sleep_on_waitq(atomic_t *wq, int retries)
{
	int do_print = 1;

again:
	if (futex(&wq->counter, FUTEX_WAIT, -1, NULL, NULL, 0) == 0)
		return 0;
	switch(errno) {
	case ETIMEDOUT:
		log_error("FUTEX_WAIT");
		break;
	case ERESTART:
		if (do_print && do_print != ERESTART) {
			log("INFO", "RESTARTING FUTEX_WAIT (I think I was FROZEN)\n");
			do_print = ERESTART; /* primitive log-spam prevention */
		}
		if (!retries) {
			log_error("FUTEX_WAIT ERESTART too many times\n");
			break;
		}
		retries--;
		goto again;
	case EAGAIN: /* EWOULDBLOCK */
		if (do_print && do_print != EAGAIN) {
			log("INFO", "FUTEX_WAIT EAGAIN\n");
			do_print = EAGAIN; /* primitive log-spam prevention */
		}
		if (!retries) {
			log_error("FUTEX_WAIT EAGAIN too many times\n");
			break;
		}
		retries--;
		goto again;
		break;
	case EINTR:
		if (do_print && do_print != EINTR) {
			log("INFO", "FUTEX_WAIT EINTR\n");
			do_print = EINTR; /* primitive log-spam prevention */
		}
		if (!retries) {
			log_error("FUTEX_WAIT EINTR too many times\n");
			break;
		}
		retries--;
		goto again;
		break;
	case EACCES:
		log("FAIL", "FUTEX_WAIT EACCES - no read access to futex memory\n");
		break;
	case EFAULT:
		log("FAIL", "FUTEX_WAIT EFAULT - bad timeout timespec address or futex address\n");
		break;
	case EINVAL:
		log("FAIL", "FUTEX_WAIT EINVAL - undefined futex operation\n");
		break;
	case ENOSYS:
		log("FAIL", "FUTEX_WAIT ENOSYS - undefined futex operation\n");
		break;
	case EDEADLK:
		log("FAIL", "FUTEX_WAIT EDEADLK - avoided deadlock\n");
		break;
	default:
		log_error("FUTEX_WAIT unexpected error (missing from man page)");
		break;
	}
	return -1;
}

int wake_waitq(atomic_t *wq, int retries)
{
	unsigned int woken = 0;
	int ret;

	atomic_set(wq, 1);
	do {
		ret = futex(&wq->counter, FUTEX_WAKE, N - 1 - woken, NULL, NULL, 0);
		retries--;
		if (ret > 0)
			woken += ret;
	} while (retries && woken < N - 1);

	if (woken < N - 1) {
		log("WARN", "Could not wake %d children. Woke %d instead. waitq: %d\n", N - 1, woken, atomic_read(waitq));
		log_error("     ");
	}
	return -1;
}

int do_lock_contended_pi_futex(int retries)
{
	int do_print = 1;

	__sync_or_and_fetch(&pi_futex->counter, FUTEX_WAITERS);
again:
	if (futex(&pi_futex->counter, FUTEX_LOCK_PI, atomic_read(pi_futex),
	      NULL, NULL, 0) == 0)
		return 0;
	switch(errno) {
	case ETIMEDOUT:
		log("WARN", "FUTEX_LOCK_PI unexpected ETIMEDOUT\n");
		break;
	case ERESTART:
		if (do_print && do_print != ERESTART) {
			log("INFO", "RESTARTING FUTEX_LOCK_PI\n");
			do_print = ERESTART; /* primitive log-spam prevention */
		}
		if (!retries) {
			log("FAIL", "locking contended pi futex returned ERESTART too many times.\n");
			break;
		}
		retries--;
	case EAGAIN: /* EWOULDBLOCK */
		if (do_print && do_print != EAGAIN) {
			log("INFO", "locking contended pi futex returned EAGAIN\n");
			do_print = EAGAIN; /* primitive log-spam prevention */
		}
		if (!retries) {
			log("FAIL", "locking contended pi futex returned EAGAIN too many times\n");
			break;
		}
		retries--;
		goto again;
	case EINTR:
		if (do_print && do_print != EINTR) {
			log("INFO", "FUTEX_LOCK_PI EINTR\n");
			do_print = EINTR; /* primitive log-spam prevention */
		}
		if (!retries) {
			log("FAIL", "locking contended pi futex returned EINTR too many times.\n");
			break;
		}
		retries--;
		goto again;
	case EACCES:
		log("FAIL", "FUTEX_LOCK_PI EACCES - no read access to futex memory\n");
		break;
	case EFAULT:
		log("FAIL", "FUTEX_LOCK_PI EFAULT - bad timeout timespec address or futex address\n");
		break;
	case EINVAL:
		log("FAIL", "FUTEX_LOCK_PI EINVAL - undefined futex operation\n");
		break;
	case ENOSYS:
		log("FAIL", "FUTEX_LOCK_PI ENOSYS - undefined futex operation\n");
		break;
	default:
		log_error("FUTEX_LOCK_PI unexpected error (missing from man page)");
		break;
	}
	return -1;
}

int do_unlock_contended_pi_futex(void)
{
	if (futex(&pi_futex->counter, FUTEX_UNLOCK_PI, 1, NULL, NULL, 0) == 0)
		return 0;

	/*
	 * There are still some lower priority waiters we failed to
	 * wake for some reason. Documentation/pi-futex.txt fails
	 * to mention what FUTEX_UNLOCK_PI returns!
	 */
	switch(errno) {
	case ERESTART:
	case EINTR:
		log("INFO", "retrying release_pi_futex since interrupted\n");
		return 1;
	case EFAULT: /* We specified the wrong pi_futex address. */
		log("FAIL", "wrong futex address or page fault/futex race in-kernel.\n");
		break;
	case EINVAL:
		/*
		 * The old value is wrong. We should never
		 * get this since the kernel ignores the val
		 * passed through sys_futex().
		 */
		log("FAIL", "kernel got confused and lost the old futex value.\n");
		break;
	case EPERM:
		/*
		 * We are unable to release the futex.
		 * We may not be holding it like we think
		 * we do.
		 */
		log_error("This process seems to lack permission to release a futex it expects to be holding. Maybe it's not being held?\n");
		break;
	case EAGAIN:
		/*
		 * Task holding the futex is exiting. Odd,
		 * that's us!
		 */
		log("FAIL", "kernel insists we're exiting but we're really not!\n");
		break;
	case ENOMEM:
		log_error("");
		break;
	case ESRCH:
		/*
		 * Task that held the futex is no more?! But
		 * that's us!
		 */
		log("FAIL", "The kernel can't seem to find this process! I sense impending doom!\n");
		break;
	}

	return -1;
}

/* Calculate the static priority to assign to a child */
int child_static_priority(int child_num)
{
	return prio_min + child_num; /* inverted: + (N - 1 - child_num);*/
}

void release_pi_futex(int *retries, int *retval, pid_t tid)
{
	int pi_val;

	do {
		/* Release the futex */
		pi_val = atomic_cmpxchg(pi_futex, tid, 0);
		if (pi_val != tid) {
		    switch (do_unlock_contended_pi_futex()) {
		    case -1: /* error -- we already logged the details */
			    *retval = -100;
			    retries--;
		    case 0: /* ok */
			    goto released;
		    case 1: /* try again */
			    if (retries) {
				    retries--;
				    continue;
			    }
			    *retval = -101;
			    break;
		    }
		} /* else we were the last to hold the futex */
	} while(retries);
released:

	log("INFO", "exited the critical section\n");
}

int kid(void *child_num_as_pointer)
{
	pid_t tid = gettid();
	int child_num = (long)child_num_as_pointer;
	int my_prio = child_static_priority(child_num);
	int held_prio = 0;
	int retval = -1;
	int retries = 100;
	int pi_val;

	if (sched_getscheduler(tid) != sched_policy) {
		log_error("failed to set scheduler policy of children.\n");
		return retval;
	}
	retval--;
	if (set_my_static_priority(my_prio)) {
		log_error("setpriority:");
		return retval;
	}
	retval--;

	/* WARN_ON(held_prio != my_prio); */
	if (get_my_static_priority(&held_prio)) {
		log_error("getpriority:");
		return retval;
	}
	retval--;
	if (my_prio != held_prio) {
		log("WARN", "Unexpected priority. Tried to set %d but got %d.\n", my_prio, held_prio);
	}
	retval --;

	if (child_num > 0) {
		atomic_inc(&dumb_barrier[0]); /* 1 */
		/* race between inc of waitq and futex()?? */
		if (sleep_on_waitq(waitq, retries) != 0) {
			log("FAIL", "unable to sleep on waitq.\n");
			retval--;
			goto out;
		}
		retval--;

		/*
		 * Now we attempt to acquire the pi futex. We should find
		 * ourselves contending on it.
		 */
		pi_val = atomic_cmpxchg(pi_futex, 0, tid);
		if (pi_val == tid) {
			log("WARN", "found uncontended pi futex.\n");
			release_pi_futex(&retries, &retval, tid);
			goto out;
		}
		retval--;

		if (do_lock_contended_pi_futex(retries) != 0) {
			log("FAIL", "unable to lock pi futex.\n");
			goto out;
		}
		retval--;
		log("INFO", "enters the critical section with priority %d.\n", held_prio);

		/* Compare our priority to what we set above. */
		retval--;
		if (get_dynamic_priority(tid, &held_prio)) {
			log("FAIL", "could not get priority.\n");
			release_pi_futex(&retries, &retval, tid);
			goto out;
		}
		retval--;

		if (held_prio != my_prio) {
			/*
			 * We should not have elevated priority
			 * since, after the first acquisition the futex
			 * should wake the next highest priority waiter.
			 */
			log("FAIL", "Not woken in priority order.\n");
			release_pi_futex(&retries, &retval, tid);
			goto out;
		}
		log("PASS", "Woken in priority order.\n");
		retval = 0;
	} else {
		pi_val = atomic_cmpxchg(pi_futex, 0, tid);
		retval--;
		if (pi_val != 0) {
			log("FAIL", "lowest priority found contended pi futex.\n");
			goto out;
		}
		retval--;

		/* Now we have the pi futex but nobody else is waiting for it */
		for (retries = 1000; atomic_read(&dumb_barrier[0]) < (int)(N - 1);
		     retries--)
			usleep(1000);
		retval--;

		log("INFO", "Normal priorities (no inheritance): \n");
		dump_dynamic_priorities();

		log("INFO", "Waking other children to contend on pi futex.\n");
		wake_waitq(waitq, retries);
		atomic_inc(&dumb_barrier[0]); /* 1 */
		retval--;

		retries = 1000;
		do {
			/* Compare our priority to what we set above. */
			if (get_dynamic_priority(tid, &held_prio)) {
				retries = 100;
				release_pi_futex(&retries, &retval, tid);
				goto out;
			}
			usleep(1000);
			retries--;
		} while(retries && (held_prio != child_static_priority(N - 1)));

		/* checkpoint should happen here */
		log("INFO", "signaling ready for checkpointing\n");
		set_checkpoint_ready();
		while (!test_checkpoint_done()) { sleep(1); }

		log("INFO", "lowest priority priority before holding pi futex: %d, during: %d\n", my_prio, held_prio);
		log("INFO", "Inherited priorities: \n");
		dump_dynamic_priorities();
		if (held_prio >= child_static_priority(N - 1)) {
			log("PASS", "Inherited priority.\n");
			retval = 0;
		} else {
			log("FAIL", "Failed to inherit priority!\n");
			retval--;
		}
	}

	release_pi_futex(&retries, &retval, tid);
out:
	log("INFO", "exiting\n");
	/* smp_mb() ?? */
	if (retval) {
		log("FAIL", "failed with %d\n", retval);
		_exit(retval);
	}
	return retval;
}

void set_rtprio_range(void)
{
	struct rlimit lim;
	int num_tries;

	/*
	 * These are the maximums allowed by the scheduler --
	 * not the maximum prios we are permitted to set. Hence
	 * the rlimit bits below.
	 */
	prio_min = sched_get_priority_min(sched_policy);
	prio_max = sched_get_priority_max(sched_policy);
	if (prio_min < 0  || prio_max < 0) {
		log_error("sched_get_priority_min|max");
		fclose(logfp);
		exit(1);
	}

	if ((prio_max - prio_min) < 1) {
		log("FAIL", "Too fewer priority levels for this test.\n");
		fclose(logfp);
		exit(13);
	}
	if ((int)N > (prio_max - prio_min)) {
		N = prio_max - prio_min;
		log("WARN", "Fewer priority levels than specified processes. Using %d processes (the number of priority levels)\n", N);
	}

	for (num_tries = 2; num_tries; num_tries--) {

		if (getrlimit(RLIMIT_RTPRIO, &lim) == -1) {
			log_error("getrlimit");
			fclose(logfp);
			exit(12);
		}
		if (lim.rlim_cur == RLIM_INFINITY)
			break;
		if (lim.rlim_cur >= N)
			break;

		/* Else we must try to adjust the allowable range */
		if (lim.rlim_cur < N)
			lim.rlim_cur = N;
		if (lim.rlim_cur > lim.rlim_max)
			lim.rlim_max = lim.rlim_cur;
		if (setrlimit(RLIMIT_RTPRIO, &lim) == -1) {
			log_error("setrlimit");
			fclose(logfp);
			exit(10);
		}
	}

	if (getrlimit(RLIMIT_RTPRIO, &lim) == -1) {
		log_error("getrlimit");
		fclose(logfp);
		exit(13);
	}
	log("INFO", "RLIMIT_RTPRIO: soft (cur): %ld hard (max): %ld\n",
		lim.rlim_cur, lim.rlim_max);
}

int main(int argc, char **argv)
{
	struct sched_param proc_sched_param;
	pid_t finished;
	int i = 0, status = 0, excode;
	char *freezerdir;

	if (argc < 2)
		exit(1);
	freezerdir = argv[1];

	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("FAIL: couldn't open logfile");
		exit(6);
	}
	 /* redirect stdout and stderr to the log file */
	dup2(fileno(logfp), 1);
	dup2(fileno(logfp), 2);

	set_rtprio_range();

	proc_sched_param.sched_priority = prio_min;
	if (sched_setscheduler(getpid(), sched_policy,
			       &proc_sched_param) != 0) {
		log_error("sched_setscheduler");
		fclose(logfp);
		exit(3);
	}

	log("INFO", "running test with %d children\n", N);

	if (!move_to_cgroup("freezer", freezerdir, getpid())) {
		log_error("move_to_cgroup");
		fclose(logfp);
		exit(5);
	}

	kids = malloc(sizeof(pid_t)*N);
	if (kids == NULL) {
		log_error("malloc");
		fclose(logfp);
		exit(7);
	}

	/* Initialize the waitq to hold N - 1 processes */
	waitq = alloc_futex_mem(sizeof(*waitq));
	if (!waitq) {
		log_error("alloc_futex_mem");
		fclose(logfp);
		exit(8);
	}
	atomic_set(waitq, -1);

	pi_futex = alloc_futex_mem(sizeof(*pi_futex));
	if (!pi_futex) {
		log_error("alloc_futex_mem");
		fclose(logfp);
		exit(9);
	}
	atomic_set(pi_futex, 0);

	fflush(logfp);
	fflush(stderr);
	fflush(stdout);
	for (i = 0; i < (int)N; i++) {

		char *new_stack = malloc(SIGSTKSZ*8);
		kids[i] = clone(kid, new_stack + SIGSTKSZ*8, clone_flags, (void*)(long)i);
		if (kids[i] <= 0)
			break;
		log("INFO", "thread %d started.\n", kids[i]);
	}

	if (i < (int)N) {
		log_error("couldn't start N children");
		log("INFO", "killing %d child tasks.\n", i);
		for (; --i > -1;)
			kill(kids[i], SIGTERM);
		excode = 3;
		goto out;
	}

	log("INFO", "Waiting for children to finish.\n");
	excode = 0;
	do {
		/*
		 * __WALL allows us to wait for all threads to exit
		 */
		finished = waitpid(-1, &status, __WALL);
		if (!finished)
			continue;
		if ((finished == -1) && (errno == ECHILD))
			break;

		if (clone_flags & CLONE_THREAD)
			i = 0;
		else
			i--;

		log("INFO", "%d exited\n", finished);
		/* Save any [ir]regular termination info in excode. */
		if (WIFEXITED(status)) {
			log("INFO", "child %d exited with %d\n", finished,
			    WEXITSTATUS(status));
			if (!excode)
				excode = WEXITSTATUS(status);
		} else if (WIFSIGNALED(status)) {
			log("FAIL", "child %d terminated irregularly with signal %d.\n", finished, WTERMSIG(status));
			if (!excode)
				excode = WTERMSIG(status);
		}
	} while(1);
out:
	log("INFO", "Parent exiting (%d children left).\n", i);
	fflush(logfp);
	fclose(logfp);
	free(kids);
	exit(excode);
}
