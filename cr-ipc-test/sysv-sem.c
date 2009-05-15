/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
/* Create a semaphore, checkpoint, restart, etc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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

static void sem_lock(int semid)
{
	struct sembuf buf;
	int rc;

	buf.sem_num = 0;
	buf.sem_op = -1;
	buf.sem_flg = 0;

	rc = semop(semid, &buf, 1);
	if (rc == -1)
		bail("semop");
}

static void sem_unlock(int semid)
{
	struct sembuf buf;
	int rc;

	buf.sem_num = 0;
	buf.sem_op =  1;
	buf.sem_flg = 0;

	rc = semop(semid, &buf, 1);
	if (rc == -1)
		bail("semop");
}

/* fork and do checkpoint in child, then fork and attempt
 * restart in child
 */

static void checkpointer(FILE *fp, int semid)
{
	int rc;

	sem_lock(semid);
	sem_unlock(semid);

	rc = checkpoint(getpid(), fileno(fp), 0);
	if (rc < 0)
		fail("checkpoint");

	/* we checkpointed successfully and should exit */
	if (rc > 0)
		exit(0);

	sem_lock(semid);
	sem_unlock(semid);
	pass();
}

static void restarter(FILE *fp)
{
	int rc;

	rc = restart(getpid(), fileno(fp), 0);

	/* should not get here */
	fail("restart");
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

int main(int argc, char **argv)
{
	FILE *ckpt_image;
	pid_t child;
	int status;
	int semid;

	ckpt_image = tmpfile();
	if (!ckpt_image)
		bail("tmpfile");

	semid = sem_create();

	sem_lock(semid);
	sem_unlock(semid);

	/* Checkpointing terminals not supported yet */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	child = fork();
	if (child == -1)
		bail("fork");

	if (child == 0)
		checkpointer(ckpt_image, semid);

	wait(&status);

	check_child_status(status);

	/* kernel reads from the file's current position, which
	 * has been changed by checkpoint(2)
	 */
	rewind(ckpt_image);

	child = fork();
	if (child == -1)
		bail("fork");

	if (child == 0)
		restarter(ckpt_image);

	wait(&status);

	check_child_status(status);

	pass();

	return 0;
}
