/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libcrtest.h>

#define OUTFILE "/tmp/sleepout"

int main(int argc, char *argv[])
{
	printf("I am %d\n", getpid());
	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move to freezer cgroup /1\n");
		do_exit(1);
	}
	close(0);
	close(1);
	close(2);
	close(3);
	close(4);
	sleep(3);
	sleep(3);
	exit(0);
}
