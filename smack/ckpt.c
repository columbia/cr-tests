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

#define OUTFILE "./cr-test.out"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int ret;
	int n;
	char ctx[200];

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

	ret = syscall(__NR_checkpoint, pid, STDOUT_FILENO, CHECKPOINT_SUBTREE,
		-1);
	if (ret < 0) {
		perror("checkpoint");
		exit(2);
	}

	fprintf(file, "world, hello!\n");
	fprintf(file, "ret = %d\n", ret);
	fflush(file);
	file = fopen("/proc/self/attr/current", "r");
	if (!file)
		return 1;
	n = fread(ctx, 1, 200, file);
	fclose(file);
	file = fopen("./context", "w");
	if (!file)
		return 1;
	fwrite(ctx, 1, n, file);
	fclose(file);

	return 0;
}

