/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <libcrtest.h>
#include "../clone.h"

int clone_newuser;
#define VALUE "hello world"

void write_status(void)
{
	FILE *fout = fopen("sandbox/status", "w");
	if (!fout)
		return;
	fprintf(fout, "pid %d\nuid %d\n", getpid(), getuid());
	fflush(fout);
	fclose(fout);
}

void wait_on(char *fnam)
{
	struct stat statbuf;
	int ret;

	while (1) {
		ret = stat(fnam, &statbuf);
		if (ret == 0)
			return;
	}
}

int do_clone(long depth);

int do_child(void *vargv)
{
	long depth = (long) vargv;

	/* TODO: add checks for keychain here */
	if (depth)
		return do_clone(depth);
	setgid(500);
	setuid(500);
	creat("sandbox/started", 0755);
	wait_on("sandbox/go");
	write_status();
	creat("sandbox/written", 0755);
	wait_on("sandbox/die");
	exit(1);
}

int do_clone(long depth)
{
	int pid;
	int stacksize = 4*getpagesize();
	void *childstack, *stack = malloc(stacksize);
	unsigned long flags = 0;

	flags |= CLONE_NEWUSER;

	if (!stack) {
		return -1;
	}
	childstack = stack + stacksize;

	depth--;
	pid = clone(do_child, childstack, flags, (void *)depth);
	if (pid == -1) {
		return -1;
	}
	wait_on("sandbox/die");
	return 0;
}

int main(int argc, char *argv[])
{
	char *cgroup = "1";
	printf("%d\n", getpid());
	fflush(stdout);
	if (argc > 1)
		cgroup = argv[1];

	if (!move_to_cgroup("freezer", cgroup, getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}
	close(0);
	close(1);
	close(2);
	close(3);
	return do_clone(30);
}
