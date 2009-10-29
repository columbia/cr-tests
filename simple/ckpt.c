/*
 * Copyright (C) 2008 Oren Laadan
 */

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include "../cr.h"

#define OUTFILE "/tmp/cr-test.out"
#define CKPTFILE "out"
#define LOGFILE "log"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int logfd;
	int ckptfd;
	int ret;

	unlink(LOGFILE);
	logfd = open(LOGFILE, O_RDWR | O_CREAT, 0600);
	if (logfd < 0) {
		perror("open logfile");
		exit(1);
	}


	unlink(OUTFILE);
	file = fopen(OUTFILE, "w+");
	if (!file) {
		perror("open");
		exit(1);
	}

	unlink(CKPTFILE);
	ckptfd = open(CKPTFILE, O_WRONLY|O_CREAT, 0644);
	if (ckptfd < 0) {
		perror("open");
		exit(1);
	}

	/* TODO these may no longer need to be closed? */
	close(0);
	close(1);
	close(2);

	fprintf(file, "hello, world!\n");
	fflush(file);

	ret = syscall(__NR_checkpoint, pid, ckptfd, CHECKPOINT_SUBTREE, logfd);
	if (ret < 0) {
		perror("checkpoint");
		exit(2);
	}

	fprintf(file, "world, hello!\n");
	fprintf(file, "ret = %d\n", ret);
	fflush(file);
	close(logfd);

	return 0;
}

