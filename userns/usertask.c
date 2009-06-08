/*
 * Copyright (C) 2009 IBM
 * Author: Serge Halyn
 */

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include "../cr.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <libcrtest.h>

int usage(char *whoami)
{
	printf("Usage: %s [-u uid] [-g gid]\n", whoami);
	printf("  uid is the uid to run as.  If unspecified, use 501\n");
	printf("  gid is the gid to run as.  If unspecified, use 501\n");
	printf("  output file is always 'outfile'\n");
	printf("  Since this test must setuid, it must be run as root\n");
	exit(1);
}

#define DIRNAME "./sandbox"
#define ERRFILE DIRNAME "/error"
#define OUTFILE DIRNAME "/outfile"

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



int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int uid=501, gid=501;
	int ret = 0;
	int opt;
	int fd;
	int i;
	int ngrp;
	gid_t *grplist;
	DIR *d;

	if (getuid())
		usage(argv[0]);
	unlink("ckpt.out");
	unlink("outfile");

	while ((opt = getopt(argc, argv, "u:g:")) != -1) {
		switch (opt) {
		case 'u':
			uid = atoi(optarg);
			break;
		case 'g':
			gid = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Couldn't switch to cgroup /1\n");
		exit(1);
	}

	close(0);
	close(1);
	close(2);

	setgroups(0, NULL);
	setgid(gid);
	setuid(uid);

	unlink(OUTFILE);
	file = fopen(OUTFILE, "w+");
	if (!file) {
		perror("fopen");
		exit(1);
	}

	if (dup2(0,2) < 0) {
		perror("dups");
		exit(1);
	}


	creat("sandbox/started", 0755);
	wait_on("sandbox/go");

	fprintf(file, "%d %d\n", getuid(), getegid());
	ngrp = getgroups(0, NULL);
	grplist = malloc(ngrp * sizeof(gid_t));
	getgroups(ngrp, grplist);
	for (i=0; i< ngrp; i++) {
		fprintf(file, "auxgrp %d\n", grplist[i]);
	}
	d = opendir("/root");
	if (d)  /* shouldn't be allowed */ {
		creat(ERRFILE, 0755);
		fprintf(file, "I managed to opendir(/root) - OOPS\n");
	}
	fflush(file);
	fclose(file);
	creat("sandbox/readytodie", 0755);
	wait_on("sandbox/die");

	return 0;
}

