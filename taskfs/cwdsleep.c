/*
 * Copyright 2009 IBM Corp.
 * Author: Serge Hallyn
 */

#include <unistd.h>
#include <stdio.h>
#include <libcrtest.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
 * arg 1: full path of cgroup to enter
 * arg 2: 'cwd' or 'chroot'
 * arg 3: path to cd or chroot into
 */

int main(int argc, char *argv[])
{
	int ret;
	char *freezer = NULL;

	if (argc > 1) {
		freezer = argv[1];
		if (!move_to_cgroup("freezer", freezer, getpid())) {
			printf("Failed to move myself to cgroup %s\n", freezer);
			return 1;
		}
	}

	close(0);
	close(1);
	close(2);
	close(3);

	ret = creat("./ready", 0755);
	if (ret < 0)
		return 1;
	close(ret);
	sleep(300);
	return 0;
}
