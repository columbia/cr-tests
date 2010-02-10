#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#define _GNU_SOURCE
#include <sched.h>
#include "eclone-tests.h"
#include "genstack.h"

/*
 * Verify that eclone() fails if nr_pids exceeds the current nesting level
 * of pid namespaces
 */
int verbose = 0;

#define CHILD_TID1	377
#define	CHILD_TID2	399
#define	CHILD_ARG	(void *)0x979797

pid_t pids[] = { CHILD_TID1, CHILD_TID2 };
int parent_tid;
int child_tid;

int do_child(void *arg)
{
	if (verbose)
		printf("Child created with [%d, %d]\n", gettid(), getpid());

	sleep(2);
	exit(0);
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
		printf("ERROR: setup_stack returns NULL for size %d\n",
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

	errno = 0;
	args_size = sizeof(struct clone_args);
	rc = eclone(child_fn, child_arg, flags_low, &clone_args, pids);

	if (verbose) {
		printf("[%d, %d]: eclone() returned %d, error %d\n", getpid(),
				gettid(), rc, errno);
		fflush(stdout);
	}

	return rc;
}

int do_test(void *arg)
{
	int rc, pid, status;
	unsigned long flags;
	int nested_ns;
	int nr_pids;
	int error;

	nested_ns = *(int *)arg;
	nr_pids = 2;

	flags = SIGCHLD|CLONE_PARENT_SETTID|CLONE_CHILD_SETTID;

	pid = do_eclone(do_child, CHILD_ARG, flags, nr_pids, pids);

	error = 0;
	if (pid < 0)
		error = errno;

	/* If we did create a child, wait for it to exit */
	if (pid > 0) {
		rc = waitpid(pid, &status, __WALL);
		if (rc < 0) {
			printf("%d: ERROR: waitpid() rc %d, error %d\n",
					getpid(), rc, errno);
			verbose = 1;
		}
	}

	if (verbose) {
		printf("%d: nested_ns %d, pid %d, error %d\n", getpid(),
				nested_ns, pid, error);
	}

	/*
	 * We set nr_pids to 2 above. If we cloned from current pid ns,
	 * eclone() must fail with EINVAL. If we eclone() from a nested pid
	 * ns, eclone() must succeed. In all other cases, test has failed.
	 */
	rc = 0;
	if (!nested_ns && (pid < 0) && (error == EINVAL)) {
		printf("%d: PASSED: Got EINVAL when nr_pids > nesting-depth\n",
				getpid());
	} else if (nested_ns && (pid > 0)) {
		printf("%d: PASSED: eclone() succeeded in nested pid-ns, "
				"pid %d\n", getpid(), pid);
	} else {
		printf("%d: FAILED: nested_ns %d, pid %d, error %d\n", getpid(),
				nested_ns, pid, error);
		rc = 1;
	}

	fflush(stdout);
	return rc;
}

int main()
{
	int rc, pid, status;
	int nested_ns;
	unsigned long flags; 
	void *stack;

	/* First test in current pid namespace */
	nested_ns = 0;
	rc = do_test(&nested_ns);
	if (rc)
		exit(rc);

	/* Then test in a nested pid-namespace - use normal clone() */
	stack = malloc(STACKSIZE);
	if (!stack) {
		printf("ERROR: setup_stack returns NULL for size %d\n",
				STACKSIZE);
		exit(1);
	}
	stack += (STACKSIZE - 1);

	nested_ns = 1;
	flags = SIGCHLD|CLONE_NEWPID|CLONE_NEWNS;
	pid = clone(do_test, stack, flags, (void *)&nested_ns, NULL, NULL, NULL);
	if (pid < 0) {
		printf("ERROR: clone() failed, pid %d, error %s\n", pid,
				strerror(errno));
		exit(1);
	}

	rc = waitpid(pid, &status, __WALL);
	if (rc < 0) {
		printf("ERROR: waitpid() failed, rc %d, error %s\n", rc,
				strerror(errno));
		fflush(stdout);
		exit(1);
	}
	return 0;
}
