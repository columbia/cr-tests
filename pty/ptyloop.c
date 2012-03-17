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

#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif

extern int move_to_cgroup(char *subsys, char *grp, int pid);

static int posix_openpt(int flags)
{
	    return open("/dev/ptmx", flags);
}

int main(int argc, char *argv[])
{
	pid_t pid;
	char buf[5];
	FILE *file;
	int ret;
	int fd;

	if (!move_to_cgroup("freezer", "1", getpid())) {
		printf("Failed to move myself to cgroup /1\n");
		exit(1);
	}
	fd = posix_openpt(O_RDWR);
	printf("openpt gave me %d, %s\n", fd, ptsname(fd));
	if (fd < 0) {
		perror("openpt");
		exit(1);
	}
	unlockpt(fd);
	pid = fork();
	printf("fork gave me %d\n", pid);
	if (pid == 0) {
		int fd2 = open(ptsname(fd), O_RDWR);
		if (fd2 < 0) {
			perror("open");
			exit(1);
		}
		close(0);
		close(1);
		close(2);
		dup2(fd2, 0);
		dup2(fd2, 1);
		dup2(fd2, 2);
		while (1) {
			write(fd2, "hi", 3);
			sleep(1);
		}
	}
	if (pid < 0) {
		perror("fork");
		exit(1);
	}
	close(0);
	close(1);
	close(2);
	while (1) {
		int f;
		ret = read(fd, buf, 3);
		if (ret == 3)
			f = creat("./read-ok", S_IRUSR|S_IWUSR);
		else
			f = creat("./read-bad", S_IRUSR|S_IWUSR);
		close(f);
	}

	return 0;
}

