/*
 * Copyright 2009 IBM Corporation
 * Author: Serge Hallyn <serue@us.ibm.com>
 *
 * Move to freezer cgroup 1.
 * Create a sysv sem.  Sleep 3 seconds to allow ourselves
 * to be checkpointed.  Then check whether the sem is still
 * there.  If so, then create ./sem-ok
 */
#include <grp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libcrtest.h>

int semid, semid2;

/* create two semaphores, one private, one not */
void create_sems(int mode)
{
	semid = semget(IPC_PRIVATE, 1, mode);
	semid2 = semget(0xabcd10f, 1, IPC_CREAT | mode);
	if (semid == -1) {
		perror("semget IPC_PRIVATE");
		exit (1);
	}
	if (semid2 == -1) {
		perror("semget 0xabcd10f");
		exit (1);
	}
}

int check_sems(void)
{
	int ret;

	ret = semctl(semid, 0, GETVAL);
	if (ret == -1)
		return 0;
	ret = semctl(semid2, 0, GETVAL);
	if (ret == -1)
		return 0;
	return 1;
}

void dosetuid(int uid)
{
	int ret;
	setgroups(0, NULL);
	ret = setgid(uid);
	if (ret == -1) {
		perror("setgid");
		exit(1);
	}
	ret = setuid(uid);
	if (ret == -1) {
		perror("setuid");
		exit(1);
	}
}

void docreat(char *fnam, int mode)
{
	int ret = creat(fnam, mode);
	if (ret == -1) {
		perror("create"); /* though noone can hear us */
		exit(2);
	}
}

void usage(char *me)
{
	printf("Usage: %s [-e] [-r] [-u uid]\n", me);
	printf("   if uid is specified, switch to uid after creating sems\n");
	printf("   if -e is specified, switch to uid before create sems\n");
	printf("   if -r is specified, use 755 mode to create (default 600)\n");
	exit(1);
}

#define DIRNAME "./sandbox"
#define SEMCREATED DIRNAME "/sem-created"
#define FNAME DIRNAME "/sem-ok"

int main(int argc, char *argv[])
{
	int uid = -1;
	int early = 0;
	int read = 0;
	int mode = 0600;
	int c;

	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}

	while ((c = getopt(argc, argv, "reu:")) != -1) {
		switch(c) {
		case 'u': uid = atoi(optarg); break;
		case 'e': early = 1; break;
		case 'r': read = 1; break;
		default: usage(argv[0]);
		}
	}
	mkdir(DIRNAME, 0755);
	if (uid != -1)
		chown(DIRNAME, uid, 0);

	if (early && uid == -1)
		usage(argv[0]);

	if (early)
		dosetuid(uid);

	if (read)
		mode = 0755;
	create_sems(mode);

	docreat(SEMCREATED,  S_IRUSR | S_IWUSR);

	close(0);
	close(1);
	close(2);
	close(3);

	if (!early && uid!=-1)
		dosetuid(uid);

	sleep(3);


	if (check_sems())
		docreat(FNAME,  S_IRUSR | S_IWUSR);
	exit(0);
}
