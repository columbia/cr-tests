/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */

#include <unistd.h>
#include <stdio.h>
#include <libcrtest.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sched.h>

int child(void *data)
{
	int ret;

	ret = chroot(".");
	if (ret < 0)
		return 1;
	ret = creat("./ready", 0755);
	if (ret < 0)
		return 1;
	close(ret);
	sleep(300);

	return 0;
}

int main(int argc, char *argv[])
{
	int ret, pid;
	int status;
	char *freezer = "1";

	if (argc > 1)
		freezer = argv[1];

	if (!move_to_cgroup("freezer", freezer, getpid())) {
		printf("Failed to move myself to cgroup %s\n", freezer);
		return 1;
	}

	close(0);
	close(1);
	close(2);
	close(3);

	pid = fork();
	if (pid < 0)
		return pid;
	if (pid == 0)
		child(NULL);
	ret = waitpid(pid, &status, 0);
	return ret;
}
