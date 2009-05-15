/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
/* Ensure that sysv IPC resources are not visible in a new IPC namespace.
 * This testcase must run as root.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

#include "checkpoint.h"
#include "util.h"

/* Create a binary semaphore (mutex) */
static int sem_create(void)
{
	int semid;
	int rc;

	semid = semget(IPC_PRIVATE, 1, (S_IRUSR | S_IWUSR));
	if (semid == -1)
		bail("semget");

	rc = semctl(semid, 0, SETVAL, 1);
	if (rc == -1)
		bail("semctl");

	return semid;
}

static void child_func(int semid)
{
	int val;

	val = semctl(semid, 0, GETVAL);
	if (val == -1)
		pass();

	errno = 0;
	fail("semaphore addressable in new IPC namespace");
}

static void check_child_status(int status)
{
	if (!WIFEXITED(status))
		fail("child exited abnormally");

	switch WEXITSTATUS(status) {
		case 0:
			/* proceed */
			return;
			break;
		case 1:
			/* fail */
			fail("child exited with status 1");
			break;
		case 2:
			bail("child exited with status 2");
			break;
		default:
			bail("child exited with unknown status");
			break;
		}
}

#ifndef CLONE_NEWIPC
#define CLONE_NEWIPC 0x08000000
#endif

static const int cloneflags = CLONE_NEWIPC | SIGCHLD;

static int do_clone(void)
{
	return syscall(SYS_clone, cloneflags, NULL);
}

int main(int argc, char **argv)
{
	pid_t child;
	int status;
	int semid;

	/* FIXME: should check for root/CAP_SYS_ADMIN */

	semid = sem_create();

	child = do_clone();
	if (child == -1)
		bail("clone");

	if (child == 0)
		child_func(semid);

	wait(&status);

	check_child_status(status);

	pass();

	/* FIXME: delete semaphore */

	return 0;
}
