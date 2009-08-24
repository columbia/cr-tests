/*
 * Copyright 2009 IBM Corp.
 * Author: Matt Helsley <matthltc@us.ibm.com>
 *
 * Test the contended case of simple futex operations by causing a bunch of
 * tasks to WAIT. Then, after checkpoint WAKE them all at once.
 *
 * NOTE: The only other non-deprecated/non-racy futex operation which may
 * need further testing across checkpoint/restart is FUTEX_CMP_REQUEUE. However,
 * it's supposed to be much like WAKE in that it WAKEs N tasks. So, until we
 * test it, we might suspect it would have similar issues (if any) to WAKE.
 * (See futex(2) and futex(7))
 *
 * See README for information on interpretting the log output.
 */
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <asm/mman.h> /* for PROT_SEM */
#include <linux/futex.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>

#include "libfutex/libfutex.h"
#include "libfutex/atomic.h"
#include "libcrtest/libcrtest.h"

#define LOG_FILE	"log.plain"
FILE *logfp = NULL;
atomic_t log_lock = { 0 };

/* number of child processes to WAIT on futex */
#define N 3

const int clone_flags = CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_VM|CLONE_SYSVSEM|SIGCHLD; /* !CLONE_THREAD because we want to wait for the children */

/* These record the progress of the children so we can dump it for checkpoint */
atomic_t dumb_barrier[2] = { {0}, {0} };

atomic_t *test_futex; /* simulating already-contended test_futex */

int kid(void *trash)
{
	atomic_inc(&dumb_barrier[0]); /* 1 */
again:
	if (futex(&test_futex->counter, FUTEX_WAIT, -1, NULL, NULL, 0) != 0) {
		switch(errno) {
			case ETIMEDOUT:
				log_error("FUTEX_WAIT ETIMEDOUT");
				break;
			case ERESTART:
				log("INFO", "RESTARTING FUTEX_WAIT (I think I was FROZEN)");
				goto again;
			case EAGAIN: /* EWOULDBLOCK */
				if (atomic_read(test_futex) == 1) {
					log("INFO", "kid: I was interrupted.\n");
					log("INFO", "kid: and now test_futex==1, so I'm done\n");
					break;
				}
				log("INFO", "FUTEX_WAIT EAGAIN");
				goto again;
				break;
			case EINTR:
				log("INFO", "FUTEX_WAIT EINTR");
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
			default:
				log_error("FUTEX_WAIT unexpected error (missing from man page)");
				break;
		}
	}
	atomic_inc(&dumb_barrier[1]); /* 2 */
	return 0;
}

void dump (const char *prefix)
{
	log("INFO","%s children past 1: %d\t children past 2: %d\t futex: %d\n",
	       prefix,
	       atomic_read(&dumb_barrier[0]),
	       atomic_read(&dumb_barrier[1]),
	       atomic_read(test_futex));
}

void sig_dump(int signum)
{
	dump("Interrupt sample:");
}

int main(int argc, char **argv)
{
	pid_t kids[N];
	int i, num_killed = 0, excode = EXIT_FAILURE;

	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}
	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("could not open logfile");
		exit(1);
	}
	dup2(fileno(logfp), 1); /* redirect stdout and stderr to the log file */
	dup2(fileno(logfp), 2);

	test_futex = alloc_futex_mem(sizeof(*test_futex));
	if (!test_futex) {
		log_error("alloc_futex_mem");
		exit(3);
	}
	atomic_set(test_futex, -1);

	signal(SIGINT, sig_dump);
	for (i = 0; i < N; i++) {
		char *new_stack = malloc(SIGSTKSZ*8);
		if (!new_stack) {
			i--;
			break;
		}
		kids[i] = clone(kid, new_stack + SIGSTKSZ*8, clone_flags,
				NULL);
		if (kids[i] < 0) {
			i--;
			break;
		}
	}

	if (i < N) {
		log_error("N x FUTEX_WAIT");
		log("INFO", "killing %d child tasks.\n", i);
		for (; --i > -1;)
			kill(kids[i], SIGTERM);
		_exit(4);
	}

	/* parent */
	log("INFO", "Waiting for children to sleep on futex\n");
	while (atomic_read(&dumb_barrier[0]) != N) /* 1 */
		sleep(1);
	dump("After 1, before 2:");

	sleep(1);
	log("INFO", "signaling ready for checkpointing\n");
	set_checkpoint_ready();
	while (!test_checkpoint_done()) { sleep(1); }

	log("INFO", "Parent woken\n");
	atomic_set(test_futex, 1);
	dump("After 1, cleared test_futex, before 2:");
	i = futex(&test_futex->counter, FUTEX_WAKE, N, NULL, NULL, 0);
	if (i == -1) {
		fprintf(logfp, "futex_wake N=%d returned %d\n", N, i);
		log_error("FUTEX_WAKE");
		sleep(1); /* wait for all woken tasks to exit quietly */

		/* kill the rest */
		for (i = 0; i < N; i++) {
			if (kill(kids[i], SIGKILL) == 0)
				num_killed++;
		}
		if (num_killed)
			log("INFO", "killed %d remaining child tasks.\n",
				num_killed);
	} else
		excode = EXIT_SUCCESS;
	dump("After 2:");

	do_wait(N);
	dump("After 3:");
	fclose(logfp);
	exit(excode);
}
