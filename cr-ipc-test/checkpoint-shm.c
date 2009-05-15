/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
/* Checkpoint self, and then restart.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "checkpoint.h"
#include "util.h"

/* fork and do checkpoint in child, then fork and attempt
 * restart in child
 */

static const char *shm_string = "deadbeef";

static void checkpointer(FILE *fp)
{
	size_t shmsize = strlen(shm_string) + 1;
	void *shmptr;
	int shmid;
	int rc;

	shmid = shmget(IPC_PRIVATE, shmsize, (S_IRUSR | S_IWUSR));
	if (shmid == -1)
		bail("shmget");

	shmptr = shmat(shmid, NULL, 0);
	if (shmptr == (void *)-1)
		bail("shmat");

	memcpy(shmptr, shm_string, shmsize);

	rc = checkpoint(getpid(), fileno(fp), 0);
	if (rc < 0)
		fail("checkpoint");

	/* checkpoint successful, exit normally */
	if (rc != 0)
		exit(0);

	/* else we were restarted */
	if (memcmp(shmptr, shm_string, shmsize) != 0)
		fail("shared memory contents miscompare");

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
	pid_t child;
	int status;
	FILE *ckpt_image;

	ckpt_image = tmpfile();
	if (!ckpt_image)
		bail("tmpfile");

	/* Checkpointing terminals not supported yet */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	child = fork();
	if (child == -1)
		bail("fork");

	if (child == 0)
		checkpointer(ckpt_image);

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
