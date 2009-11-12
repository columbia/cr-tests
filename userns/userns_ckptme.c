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
#include <sys/syscall.h>

#include <libcrtest.h>
#include "../clone.h"

int clone_newuser;

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

int do_child(void *vargv)
{
	setgid(500);
	setuid(500);
	creat("sandbox/started", 0755);
	wait_on("sandbox/go");
	write_status();
	creat("sandbox/written", 0755);
	wait_on("sandbox/die");
	exit(1);
}

int do_clone()
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

	pid = clone(do_child, childstack, flags, NULL);
	if (pid == -1) {
		return -1;
	}
	wait_on("sandbox/die");
	return 0;
}

int main(int argc, char *argv[])
{
	char *freezer  = "1";
	if (argc > 1)
		freezer = argv[1];

	printf("%d\n", getpid());
	fflush(stdout);
	if (!move_to_cgroup("freezer", freezer, getpid())) {
		printf("Failed to move myself to cgroup %s\n", freezer);
		exit(1);
	}
	close(0);
	close(1);
	close(2);
	close(3);
	return do_clone();
}
