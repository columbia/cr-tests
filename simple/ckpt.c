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

#define DEFDIR "/tmp"
#define OUTFILE "cr-test.out"
#define CKPTFILE "out"
#define LOGFILE "log"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int logfd;
	int ckptfd;
	int ret;
	char buf[400], *dir;

	if (argc < 2)
		dir = DEFDIR;
	else
		dir = argv[1];

	snprintf(buf, 400, "%s/%s", dir, LOGFILE);
	unlink(buf);
	logfd = open(buf, O_RDWR | O_CREAT, 0600);
	if (logfd < 0) {
		perror("open");
		printf("error opening logfile %s", buf);
		exit(1);
	}

	snprintf(buf, 400, "%s/%s", dir, OUTFILE);
	unlink(buf);
	file = fopen(buf, "w+");
	if (!file) {
		perror("open");
		printf("error opening outfile %s", buf);
		exit(1);
	}

	snprintf(buf, 400, "%s/%s", dir, CKPTFILE);
	unlink(buf);
	ckptfd = open(buf, O_WRONLY|O_CREAT, 0644);
	if (ckptfd < 0) {
		perror("open");
		printf("error opening ckptfile %s", buf);
		exit(1);
	}

	/* TODO these may no longer need to be closed? */
	close(0);
	dup2(fileno(file), 1);
	dup2(fileno(file), 2);

	fprintf(file, "Invoking checkpoint syscall... ");
	fflush(file);

	ret = syscall(__NR_checkpoint, pid, ckptfd, CHECKPOINT_SUBTREE, logfd);
	if (ret < 0) {
		fprintf(file, " FAILED.\n");
		perror("checkpoint");
		exit(2);
	}

	fprintf(file, "PASSED.\nret = %d\n", ret);
	fflush(file);
	close(logfd);

	return 0;
}

