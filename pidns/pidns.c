#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/wait.h>

void write_my_pid(int which)
{
	char fnam[20];
	FILE *f;

	sprintf(fnam, "./mypid.%d", which);
	f = fopen(fnam, "w");
	fprintf(f, "%d", (int) syscall(__NR_gettid));
	fclose(f);
}

int restarted(void)
{
	struct stat statbuf;
	int ret;

	ret = stat("./checkpoint-done", &statbuf);
	if (ret < 0)
		return 0;
	return 1;
}

void wait_on_restart(void)
{
	int fd;

	write_my_pid(1);
	fd = creat("./checkpoint-ready", 0600);
	close(fd);
	while (!restarted())
		sleep(1);
	write_my_pid(2);
}

int child(void *arg)
{
	int pid, status;

	pid = fork();
	if (pid == 0) {
		pid = fork();
		if (pid == 0) {
			pid = fork();
			if (pid == 0) {
				wait_on_restart();
				return 0;
			} else waitpid(pid, &status, 0);
		} else exit(1);
	} else sleep(10);
	return 0;
}

int main()
{
	int pid;
	int status;
	int stacksize = 4*getpagesize();
	void *childstack, *stack = malloc(stacksize);

	if (!stack) {
		return -1;
	}
	childstack = stack + stacksize;

	close(0);
	close(1);
	close(2);
	close(3);

	pid = clone(child, childstack, CLONE_NEWPID|SIGCHLD,
			(void *) 1);
	waitpid(pid, &status, 0);
	return 0;
}
