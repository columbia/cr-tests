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
#include <shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <libcrtest.h>

int shmid1, shmid2;

const char *shm_string = "deadbeef";

void create_shms(int mode)
{
	size_t shmsize = strlen(shm_string) + 1;
	shmid1 = shmget(IPC_PRIVATE, shmsize, mode);
	if (shmid1 == -1) {
		perror("shmget IPC_PRIVATE");
		exit (1);
	}
	shmid2 = shmget(0xabcd10f, shmsize, IPC_CREAT | mode);
	if (shmid2 == -1) {
		perror("shmget 0xabcd10f");
		exit (1);
	}
}

int check_shms(void)
{
	void *shmptr;

	shmptr = shmat(shmid1, NULL, 0);
	if (shmptr == (void *)-1)
		return 0;
	shmdt(shmptr);
	shmptr = shmat(shmid2, NULL, 0);
	if (shmptr == (void *)-1)
		return 0;
	shmdt(shmptr);
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
	printf("   if uid is specified, switch to uid after creating shms\n");
	printf("   if -e is specified, switch to uid before create shms\n");
	printf("   if -r is specified, use 755 mode to create (default 600)\n");
	exit(1);
}

#define DIRNAME "./sandbox"
#define SHMCREATED DIRNAME "/shm-created"
#define FNAME DIRNAME "/shm-ok"

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
	create_shms(mode);
	docreat(SHMCREATED,  S_IRUSR | S_IWUSR);

	close(0);
	close(1);
	close(2);
	close(3);

	if (!early && uid!=-1)
		dosetuid(uid);

	sleep(3);


	if (check_shms())
		docreat(FNAME,  S_IRUSR | S_IWUSR);
	exit(0);
}
