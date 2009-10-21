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
#define LOGFILE "log"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int logfd;
	int ret;

	unlink(LOGFILE);
	logfd = open(LOGFILE, O_RDWR | O_CREAT, 0600);
	if (logfd < 0) {
		perror("open logfile");
		exit(1);
	}

	close(0);
	close(2);

	unlink(OUTFILE);
	file = fopen(OUTFILE, "w+");
	if (!file) {
		perror("open");
		exit(1);
	}

	if (dup2(0,2) < 0) {
		perror("dups");
		exit(1);
	}

	fprintf(file, "hello, world!\n");
	fflush(file);

	ret = syscall(__NR_checkpoint, pid, STDOUT_FILENO, CHECKPOINT_SUBTREE, logfd);
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

