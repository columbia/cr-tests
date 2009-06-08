/*
 * Copyright 2009 IBM Corporation
 * Author: Serge Hallyn <serue@us.ibm.com>
 *
 * Move to freezer cgroup 1.
 * Create a sysv sem.  Sleep 3 seconds to allow ourselves
 * to be checkpointed.  Then check whether the sem is still
 * there.  If so, then create ./sem-ok
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#define __USE_GNU
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int msgid1, msgid2;

#define MSG1 "message1"
#define MSG2 "message2"

/* create two semaphores, one private, one not */
void create_mqs(void)
{
	int ret;
	struct msgbuf *b = malloc(100);

	msgid1 = msgget(IPC_PRIVATE, 0600);
	msgid2 = msgget(0xabcd10f, IPC_CREAT | 0600);
	if (msgid1 == -1) {
		perror("semget IPC_PRIVATE");
		free(b);
		exit (1);
	}
	if (msgid2 == -1) {
		perror("semget 0xabcd10f");
		free(b);
		exit (1);
	}

	b->mtype = 1;
	strcpy(b->mtext, MSG1);
	ret = msgsnd(msgid1, b, strlen(MSG1)+1, 0);
	if (ret == -1) {
		perror("msgsnd 1");
		free(b);
		exit (1);
	}
	strcpy(b->mtext, MSG2);
	ret = msgsnd(msgid2, b, strlen(MSG2)+1, 0);
	if (ret == -1) {
		perror("msgsnd 1");
		free(b);
		exit (1);
	}
	free(b);
}

int verify_queues(void)
{
	int ret;
	struct msgbuf *b = malloc(100);

	ret = msgrcv(msgid1, b, strlen(MSG1)+1, 1, 0);
	if (ret == -1 || strcmp(b->mtext, MSG1)) {
		printf("msgrcv 1 returned %d msg %s\n", ret, b->mtext);
		free(b);
		return 0;
	}
	ret = msgrcv(msgid2, b, strlen(MSG2)+1, 1, 0);
	if (ret == -1 || strcmp(b->mtext, MSG2)) {
		printf("msgrcv 2 returned %d msg %s\n", ret, b->mtext);
		free(b);
		return 0;
	}
	free(b);
	return 1;
}

void dosetuid(int uid)
{
	int ret = setuid(uid);
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
	printf("Usage: %s [-e] [-u uid]\n", me);
	printf("   if uid is specified, switch to uid after creating sems\n");
	printf("   if -e is specified, switch to uid before create sems\n");
	exit(1);
}

#define DIRNAME "./sandbox"
#define MQCREATED DIRNAME "/mq-created"
#define FNAME DIRNAME "/msq-ok"

int main(int argc, char *argv[])
{
	int uid = -1;
	int early = 0;
	int c;

	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}

	while ((c = getopt(argc, argv, "eu:")) != -1) {
		switch(c) {
		case 'u': uid = atoi(optarg); break;
		case 'e': early = 1; break;
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

	create_mqs();
	docreat(MQCREATED,  S_IRUSR | S_IWUSR);

	close(0);
	close(1);
	close(2);
	close(3);

	if (!early && uid!=-1)
		dosetuid(uid);

	sleep(3);


	if (verify_queues())
		docreat(FNAME,  S_IRUSR | S_IWUSR);
	exit(0);
}
