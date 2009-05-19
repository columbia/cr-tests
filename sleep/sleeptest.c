/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define OUTFILE "/tmp/sleepout"

void docgroup(void)
{
	FILE *fout = fopen("/cgroup/1/tasks", "w");
	if (!fout) {
		printf("Error moving myself into cgroup /1\n");
		exit(2);
	}
	fprintf(fout, "%d\n", getpid());
	fclose(fout);
}

int main(int argc, char *argv[])
{
	printf("I am %d\n", getpid());
	docgroup();
	close(0);
	close(1);
	close(2);
	close(3);
	close(4);
	sleep(3);
	sleep(3);
	exit(0);
}
