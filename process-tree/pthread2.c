#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libcrtest.h>
#include <pthread.h>

#define	ERROR_EXIT	((void *)1)
#define MIN_STACK_SIZE	(64 *1024)
#define LOG_PREFIX	"logs.d/pthread2"

FILE *logfp;

int num_threads = 8;
void **exp_addrs;
size_t  *exp_sizes;
int *tstatus;

static void usage(char *argv[])
{
	printf("%s [h] [-n num-threads]\n", argv[0]);
	printf("\t <num-threads> # of threads, default 5\n");
	do_exit(1);
}

pthread_attr_t *get_thread_attr(int tnum)
{
	int rc, size;
	pthread_attr_t *attr;
	void *stack;

	size = MIN_STACK_SIZE + (tnum * getpagesize());

	stack = malloc(size);
	if (!stack) {
		fprintf(logfp, "malloc(stack): error %s\n", strerror(errno));
		do_exit(1);
	}

	attr = malloc(sizeof(pthread_attr_t));
	if (!attr) {
		fprintf(logfp, "malloc(attr): error %s\n", strerror(errno));
		do_exit(1);
	}

	rc = pthread_attr_init(attr);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_init(): rc %d error %s\n", rc,
				strerror(errno));
		do_exit(1);
	}

	rc = pthread_attr_setstack(attr, stack, size);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_setstack(): rc %d error %s\n",
				rc, strerror(errno));
		do_exit(1);
	}

	return attr;
}

int get_stack_info(pthread_t tid, void **addrp, size_t *sizep)
{
	int rc;
	pthread_attr_t attr;

	rc = pthread_getattr_np(tid, &attr);
	if (rc < 0) {
		fprintf(logfp, "pthread_getattr_np failed, rc %d, %s\n", rc,
				strerror(errno));
		pthread_exit(ERROR_EXIT);
	}

	rc = pthread_attr_getstack(&attr, (void **)addrp, sizep);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getstackaddr failed, rc %d, %s\n",
					rc, strerror(errno));
		pthread_exit(ERROR_EXIT);
	}

	return 0;
}

void *do_work(void *arg)
{
	long tnum = (long)arg;
	int rc;
	void *act_addr;
	size_t act_size;

	fprintf(logfp, "%ld: Thread %lu: waiting for checkpoint\n", tnum,
			pthread_self());
	fflush(logfp);

	while(!test_done())
		sleep(1);

	rc = get_stack_info(pthread_self(), &act_addr, &act_size);
	if (rc < 0)
		pthread_exit(ERROR_EXIT);

	if (act_addr != exp_addrs[tnum] || act_size != exp_sizes[tnum]) {
		fprintf(logfp, "%d: Expected: (%p, %d), actual (%p, %d)\n",
				tnum, exp_addrs[tnum], exp_sizes[tnum],
				act_addr, act_size);
		fflush(logfp);
		rc = 1;
	}

	fprintf(logfp, "%d: Thread %lu: exiting, rc %d\n", tnum,
			pthread_self(), rc);
	fflush(logfp);

	tstatus[tnum] = rc;
	pthread_exit((void *)&tstatus[tnum]);
}

pthread_t *create_threads(int n)
{
	long i;
	int rc;
	pthread_t *tid_list;
	pthread_t tid;
	pthread_attr_t *attr;

	tid_list = (pthread_t *)malloc(n * sizeof(pthread_t));
	exp_addrs = malloc(sizeof(void *) * n);
	exp_sizes = malloc(sizeof(size_t) * n);
	tstatus = malloc(sizeof(int) * n);

	if (!tid_list || !exp_addrs || !exp_sizes || !tstatus) {
		fprintf(logfp, "malloc() failed, n %d, error %s\n",
				n, strerror(errno));
		do_exit(1);
	}

	for (i = 0; i < n; i++) {
		attr = get_thread_attr(i);
		if (!attr)
			do_exit(1);

		rc = pthread_create(&tid, attr, do_work, (void *)i);
		if (rc < 0) {
			fprintf(logfp, "pthread_create(): i %d, rc %d, "
					"error %s\n", i, rc, strerror(errno));
			do_exit(1);
		}

		rc = get_stack_info(tid, &exp_addrs[i], &exp_sizes[i]);
		if (rc < 0)
			do_exit(1);

		tid_list[i] = tid;
	}

	fprintf(logfp, "Created %d threads\n", n);
	fflush(logfp);

	return tid_list;
}

int wait_for_threads(pthread_t *tid_list, int n)
{
	int i;
	int rc;
	int status;
	int *statusp;
	int exit_status;

	exit_status = 0;
	for (i = 0; i < n; i++) {
		rc = pthread_join(tid_list[i], (void **)&statusp);
		if (rc < 0) {
			fprintf(logfp, "pthread_join() failed, i %d, rc %d "
					"error %s\n", i, rc, strerror(errno));
			do_exit(1);
		}

		fprintf(logfp, "i %d: *statusp %x\n", i, *statusp);
		fflush(logfp);

		if (*statusp)
			exit_status = 1;
	}

	return exit_status;
}


main(int argc, char *argv[])
{
	int c;
	int i;
	int rc;
	int status;
	pthread_t *tid_list;
	char log_file[256];

	sprintf(log_file, "%s.log", LOG_PREFIX);

	if (test_done()) {
		fprintf(stderr, "Remove %s before running test\n", TEST_DONE);
		do_exit(1);
	}


	while ((c = getopt(argc, argv, "hn:")) != EOF) {
		switch (c) {
		case 'n': num_threads = atoi(optarg); break;
		case 'h':
		default:
			usage(argv);
		}
	};

	logfp = fopen(log_file, "w");
	if (!logfp) {
		fprintf(stderr, "fopen(%s) failed, %s\n", log_file,
					strerror(errno));
		fflush(stderr);
		do_exit(1);
	}

	for (i=0; i<100; i++)  {
		if (fileno(logfp) != i)
			close(i);
	}


	tid_list = create_threads(num_threads);

	/*
	 * Now that we closed the special files and created the threads,
	 * tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	rc = wait_for_threads(tid_list, num_threads);

	do_exit(rc);
}
