#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

int do_dirty = 0;

void *growmem(int sz)
{
#ifndef USE_MALLOC
	void *m = mmap (NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
		-1, 0);
#else
	void *m = malloc (sz);
#endif
        if (do_dirty)
		memset(m, '*', sz);
	return m;
}

void usage(char *me)
{
	printf("Usage: %s [-n <numprocs>] [-m memsz] [-d memsz]\n", me);
	printf(" to put me in freezer, start me with nsexec -g\n");
	printf(" -d is like -m but dirtying the memory\n");
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
	void *v = NULL;
	if (mem) {
		v = growmem(mem);
		if (!v) {
			printf("Error growing memory child %d\n", n);
			exit(1);
		}
	}
	while (!exist_file("checkpoint-done"))
		sleep(1);
#ifndef USE_MALLOC
	if (v)
		munmap(v, mem);
#endif
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	int numprocs = 1;
	int memsz = 0;
	int i, pid, status;

	if (argc < 2)
		usage(argv[0]);
	unlink("checkpoint-done");
	unlink("ready");
	unlink("done");
	while ((c = getopt(argc, argv, "hn:m:d:")) != -1) {
		switch(c) {
		case 'n':
			numprocs = atoi(optarg);
			break;
		case 'm':
			memsz = atoi(optarg);
			break;
		case 'd':
			memsz = atoi(optarg);
			do_dirty = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
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
