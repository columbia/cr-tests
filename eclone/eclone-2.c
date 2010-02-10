#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include "eclone-tests.h"
#include "genstack.h"

/*
 * Verify that eclone() fails if the selected pid is in use.
 * We try to assign parent's pid which should already be in use.
 */
int verbose = 0;
int child_tid, parent_tid;
#define	CHILD_ARG	(void *)0x979797

pid_t pids[2];

int do_child(void *arg)
{
	printf("FAIL: Child created with [%d, %d], but we expected child "
			"creation to fail since pid is in use\n", gettid(),
 			getpid());
	exit(2);
}

static int do_eclone(int (*child_fn)(void *), void *child_arg,
		unsigned int flags_low, int nr_pids, pid_t *pids)
{
	int rc;
	void *stack;
	struct clone_args clone_args;

	stack = genstack_alloc(STACKSIZE);
	if (!stack) {
		printf("ERROR: genstack_alloc() returns NULL for size %d\n",
				STACKSIZE);
		exit(1);
	}

	memset(&clone_args, 0, sizeof(clone_args));
	clone_args.child_stack = (unsigned long)genstack_sp(stack);
	clone_args.child_stack_size = 0;
	clone_args.parent_tid_ptr = (unsigned long)(&parent_tid);
	clone_args.child_tid_ptr = (unsigned long)(&child_tid);
	clone_args.nr_pids = nr_pids;

	if (verbose) {
		printf("[%d, %d]: Parent:\n\t child_stack 0x%p, ptidp %llx, "
				"ctidp %llx, pids %p\n", getpid(), gettid(),
				stack, clone_args.parent_tid_ptr,
				clone_args.child_tid_ptr, pids);
	}

	rc = eclone(child_fn, child_arg, flags_low, &clone_args, pids);

	if (verbose) {
		printf("[%d, %d]: eclone() returned %d, error %d\n", getpid(),
				gettid(), rc, errno);
		fflush(stdout);
	}

	if (rc < 0 && errno == EAGAIN) {
		printf("PASS: Unable to create a process with a pid that is "
			"in use\n");
		exit(0);
	} else {
		printf("ERROR: eclone(): rc %d, errno %d\n", rc, errno);
		return rc;
	}
}

int main()
{
	int rc, pid, status;
	unsigned long flags;
	int nr_pids;

	flags = SIGCHLD;

	/*
	 * Try to create a process with same pid as parent !
	 */
	pids[0] = getpid();
	nr_pids = 1;

	pid = do_eclone(do_child, CHILD_ARG, flags, nr_pids, pids);

	if (verbose) {
		printf("[%d, %d]: Parent waiting for %d\n", getpid(),
					gettid(), pid);
	}

	rc = waitpid(pid, &status, __WALL);
	if (rc < 0) {
		printf("ERROR: ");
		verbose = 1;
	}

	if (verbose) {
		printf("\twaitpid(): child %d, rc %d, error %d, status 0x%x\n",
				getpid(), rc, errno, status);
		if (rc >=0) {
			if (WIFEXITED(status)) {
				printf("\t EXITED, %d\n", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				printf("\t SIGNALED, %d\n", WTERMSIG(status));
			}
		}
	}
	return 0;
}
