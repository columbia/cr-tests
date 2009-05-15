/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
/* Create new IPC namespace via clone.
 * Create and modify a semaphore in the new namespace.
 * Checkpoint and restart the child.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include "checkpoint.h"
#include "util.h"

static void check_child_status(int status)
{
	if (!WIFSIGNALED(status))
		bail("child exited in unexpected manner");

	if (WTERMSIG(status) != SIGTERM)
		bail("child received unexpected signal\n");
}

static void notify_parent_ready(const char *path)
{
	int fd;

	fd = creat(path, S_IRUSR | S_IWUSR);
	if (fd == -1)
		bail("creat");

	close(fd);
}

static void do_child(const char *path)
{
	int semid;
	int rc;

	semid = semget(IPC_PRIVATE, 1, (S_IRUSR | S_IWUSR));
	if (semid == -1)
		bail("semget");

	rc = semctl(semid, 0, SETVAL, 1);
	if (rc == -1)
		bail("semctl");

	notify_parent_ready(path);

	while (1)
		pause();

	/* expect parent to kill this task w/SIGTERM */

	exit(1);
}

/* cheesy, could at least use signals */
static void wait_for_child_ready(const char *path)
{
	struct stat buf;

	while (stat(path, &buf) != 0)
		sleep(1);
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
	const char *notify_fname;
	FILE *ckpt_image;
	pid_t child;
	int status;
	int rc;

	notify_fname = tempnam("/tmp", NULL);
	if (!notify_fname)
		bail("tempnam");

	ckpt_image = tmpfile();
	if (!ckpt_image)
		bail("tmpfile");

	/* Checkpointing terminals not supported yet */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	child = do_clone();
	if (child == -1)
		bail("clone");

	if (child == 0)
		do_child(notify_fname); /* does not return */

	wait_for_child_ready(notify_fname);

	rc = checkpoint(child, fileno(ckpt_image), 0);
	if (rc < 0)
		fail("checkpoint"); /* fixme: should kill child */

	rc = kill(child, SIGTERM);
	if (rc)
		fail("kill");

	wait(&status);

	check_child_status(status);

	/* kernel reads from the file's current position, which
	 * has been changed by checkpoint(2)
	 */
	rewind(ckpt_image);

	child = do_clone();
	if (child == -1)
		bail("clone");

	if (child == 0) {
		restart(getpid(), fileno(ckpt_image), 0);
		fail("restart");
	}

	rc = kill(child, SIGTERM);
	if (rc)
		fail("kill");

	wait(&status);

	check_child_status(status);

	pass();

	fflush(stdout);
	return 0;
}
