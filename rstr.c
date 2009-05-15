/*
 * Copyright (C) 2008 Oren Laadan
 * Changelog:
 *	early 2009: Serge: tweak usage a bit
 */

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include "cr.h"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	int ret;
	int f;

	if (argc>1) {
		f = open(argv[1], O_RDONLY);
		if (f == -1) {
			perror("open");
			return 1;
		}
	} else
		f = STDIN_FILENO;

	ret = syscall(__NR_restart, pid, f, 0);
	if (ret < 0)
		perror("restart");

	printf("should not reach here !\n");

	return 0;
}
