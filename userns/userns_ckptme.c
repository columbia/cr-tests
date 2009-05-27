/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <libcrtest.h>
#include "../clone.h"

int clone_newuser;

void write_status(void)
{
	FILE *fout = fopen("/tmp/userns.txt", "w");
	if (!fout)
		return;
	fprintf(fout, "pid %d uid %d\n", getpid(), getuid());
	fflush(fout);
	fclose(fout);
}

int do_child(void *vargv)
{
	long depth = (long)vargv;
	int status;

	if (depth) {
		do_clone((void *)(depth-1));
		wait(&status);
	}
	setgid(501);
	setuid(501);
	sleep(4);
	write_status();
	return 1;
}

int do_clone(long depth)
{
	int pid;
	int stacksize = 4*getpagesize();
	void *childstack, *stack = malloc(stacksize);
	unsigned long flags = 0;

	if (clone_newuser)
		flags |= CLONE_NEWUSER;

	if (!stack) {
		return -1;
	}
	childstack = stack + stacksize;

	pid = clone(do_child, childstack, flags, (void *)depth);
	if (pid == -1) {
		return -1;
	}
	sleep(4);
	return 0;
}

int main(int argc, char *argv[])
{
	printf("I am %d\n", getpid());
	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}
	/*
	 * give an argument to NOT clone with CLONE_NEWUSER
	 */
	if (argc == 1) {
		printf("Will clone with CLONE_NEWUSER\n");
		clone_newuser = 1;
	}
	close(0);
	close(1);
	close(2);
	close(3);
	return do_clone(3);
}
