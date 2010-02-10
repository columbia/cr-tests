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
 * Verify that eclone() fails if ->reserved0 field is non-zero.
 */
int verbose = 0;
int child_tid, parent_tid;

#define CHILD_TID1	377
#define	CHILD_ARG	(void *)0x979797

pid_t pids[] = { CHILD_TID1 };

int do_child(void *arg)
{
	printf("FAIL: Child created with [%d, %d]. We expected eclone() to "
			"fail with EINVAL since ->reserved0 is non-zero\n",
			 gettid(), getpid());
	exit(2);
}

static int do_eclone(int (*child_fn)(void *), void *child_arg,
		unsigned int flags_low, int nr_pids, pid_t *pids)
{
	int rc;
	void *stack;
	struct clone_args clone_args;
	int args_size;

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
	clone_args.reserved0 = 0xdeadbeef;
	clone_args.nr_pids = nr_pids;

	if (verbose) {
		printf("[%d, %d]: Parent:\n\t child_stack 0x%p, ptidp %llx, "
				"ctidp %llx, pids %p\n", getpid(), gettid(),
				stack, clone_args.parent_tid_ptr,
				clone_args.child_tid_ptr, pids);
	}

	args_size = sizeof(struct clone_args);
	rc = eclone(child_fn, child_arg, flags_low, &clone_args, pids);

	if (verbose) {
		printf("[%d, %d]: eclone() returned %d, error %d\n", getpid(),
				gettid(), rc, errno);
		fflush(stdout);
	}

	if (rc < 0 && errno == EINVAL) {
		printf("PASS: eclone() failed (->reserved0 is not 0)\n");
		exit(0);
	} else {
		printf("FAIL: Expected eclone() to fail with EINVAL");
		exit(1);
	}

	return rc;
}

int main()
{
	int rc, pid, status;
	unsigned long flags;
	int nr_pids = 1;

	flags = SIGCHLD;

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
