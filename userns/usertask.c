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

int usage(char *whoami)
{
	printf("Usage: %s [-u uid] [-g gid] [-e]\n", whoami);
	printf("  uid is the uid to run as.  If unspecified, use 501\n");
	printf("  gid is the gid to run as.  If unspecified, use 501\n");
	printf("  -e means wait for an external checkpoint, don't self-checkpoint\n");
	printf("  if -e is not specified, checkpoint will be written to 'ckpt.out'\n");
	printf("  output file is always 'outfile'\n");
	printf("  Since this test must setuid, it must be run as root\n");
	exit(1);
}

int do_checkpoint(pid_t pid, int fd)
{
	int ret;
	ret = syscall(__NR_checkpoint, pid, fd, 4);
	if (ret < 0) {
		perror("checkpoint");
		exit(2);
	}
	return ret;
}

int move_to_cgroup_1(void)
{
	FILE *fout = fopen("/cgroup/1/tasks", "w");
	if (!fout)
		return 1;
	fprintf(fout, "%d\n", getpid());
	fclose(fout);
	return 0;
}

#define DIRNAME "./sandbox"
int create_sandbox(int uid, int gid)
{
	int ret;

	ret = mkdir(DIRNAME, 0755);
	if (ret == -1 && errno != EEXIST) {
		perror("mkdir");
		return -1;
	}

	ret = chown(DIRNAME, uid, gid);
	if (ret == -1) {
		perror("chown");
		return -1;
	}
	return 0;
}

#define OUTFILE DIRNAME "/outfile"
#define CKPTFILE DIRNAME "/ckpt.out"

int main(int argc, char *argv[])
{
	pid_t pid = getpid();
	FILE *file;
	int uid=501, gid=501;
	int ret = 0;
	int opt;
	int external;
	int fd;
	DIR *d;

	if (getuid())
		usage(argv[0]);
	unlink("ckpt.out");
	unlink("outfile");

	while ((opt = getopt(argc, argv, "u:g:e")) != -1) {
		switch (opt) {
		case 'u':
			uid = atoi(optarg);
			break;
		case 'e':
			external = 1;
			break;
		case 'g':
			gid = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (external && move_to_cgroup_1()) {
		printf("Couldn't switch to cgroup /1\n");
		exit(1);
	}

	if (create_sandbox(uid, gid)) {
		printf("Failed to create user sandbox (%s)\n", DIRNAME);
		exit(1);
	}

	close(0);
	close(1);
	close(2);

	setgid(gid);
	setuid(uid);

	unlink(OUTFILE);
	unlink(CKPTFILE);
	file = fopen(OUTFILE, "w+");
	if (!file) {
		perror("fopen");
		exit(1);
	}

	if (dup2(0,2) < 0) {
		perror("dups");
		exit(1);
	}

	if (!external) {
		fd = open(CKPTFILE, O_RDWR|O_CREAT);
		if (fd == -1) {
			perror("open");
			exit(1);
		}
	}

	fprintf(file, "Starting (before checkpoint)\n");
	fflush(file);

	if (!external)
		ret = do_checkpoint(pid, fd);
	else
		sleep(5);

	if (ret) /* we did a checkpoint */
		exit(0);

	/* we either did a restart, or we waited on external ckpt */
	fprintf(file, "here I am, pid %d uid %d\n", getpid(), getuid());
	d = opendir("/root");
	if (d)  /* shouldn't be allowed */
		fprintf(file, "I managed to opendir(/root) - OOPS\n");
	fflush(file);
	fclose(file);
	sleep(5);
	sleep(5);

	if (d)
		return 1;
	return 0;
}
