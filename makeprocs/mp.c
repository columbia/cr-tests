#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

void *growmem(int sz)
{
	void *m = malloc(sz);
	return m;
}

void usage(char *me)
{
	printf("Usage: %s [-n <numprocs>] [-m memsz] [-i ipcsz]\n", me);
	printf(" to put me in freezer, start me with nsexec -g\n");
	exit(1);
}

int exist_file(char *fnam)
{
	struct stat bufstat;
	int ret;

	ret = stat(fnam, &bufstat);
	if (ret == 0)
		return 1;
	if (errno != ENOENT) {
		perror("stat");
		exit(1);
	}
	return 0;
}

int do_child(int n, int mem)
{
	void *v;
	if (mem) {
		v = growmem(mem);
		if (!v) {
			printf("Error growing memory child %d\n", n);
			exit(1);
		}
	}
	while (!exist_file("checkpoint-done"))
		sleep(1);
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	int numprocs = 1;
	int memsz = 0;
	int ipcsz=0;
	int i, pid, status;

	if (argc < 2)
		usage(argv[0]);
	unlink("checkpoint-done");
	unlink("ready");
	unlink("done");
	while ((c = getopt(argc, argv, "hn:m:i:")) != -1) {
		switch(c) {
		case 'n':
			numprocs = atoi(optarg);
			break;
		case 'm':
			memsz = atoi(optarg);
			break;
		case 'i':
			printf("ipc not yet handled\n");
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if (ipcsz) {
		/* alloc ipc sz */
	}

	printf("starting %d tasks with %d memory\n", numprocs, memsz);

	close(0);
	close(1);
	close(2);
	close(3);
	for (i=0; i<numprocs; i++) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(1);
		}
		if (pid == 0)
			do_child(i, memsz);
	}
	creat("ready", S_IRUSR);
	while (!exist_file("checkpoint-done"))
		sleep(1);
	for (i=0; i<numprocs; i++)
		wait(&status);
	creat("done", S_IRUSR);
	exit(0);
}
