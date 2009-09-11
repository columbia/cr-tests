#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libcrtest.h>

int num_threads = 5;
FILE *logfp;
#define LOG_PREFIX		"logs.d/pthread1"

static void usage(char *argv[])
{
	printf("%s [h] [-n num-threads]\n", argv[0]);
	printf("\t <num-threads> # of threads, default 5\n");
	do_exit(1);
}

void *
do_work(void *arg)
{
#if 0
	fprintf(logfp, "Thread %lu sleeping...\n", pthread_self());
	fflush(logfp);
#endif
	while(!test_done())
		sleep(1);
}

pthread_t *create_threads(int n)
{
	int i;
	int rc;
	pthread_t *tid_list;
	pthread_t tid;

	tid_list = (pthread_t *)malloc(n * sizeof(pthread_t));
	if (!tid_list) {
		fprintf(logfp, "malloc(%d) failed, error %s\n",
				n * sizeof(pthread_t), strerror(errno));
		do_exit(1);
	}

	for (i = 0; i < n; i++) {
		rc = pthread_create(&tid, NULL, do_work, NULL);
		if (rc < 0) {
			fprintf(logfp, "pthread_create() failed, i %d, rc %d "
					"error %s\n", i, rc, strerror(errno));
			do_exit(1);
		}
		tid_list[i] = tid;
	}

	fprintf(logfp, "Created %d threads\n", n);

	return tid_list;
}

void wait_for_threads(pthread_t *tid_list, int n)
{
	int i;
	int rc;

	for (i = 0; i < n; i++) {
		rc = pthread_join(tid_list[i], NULL);
		if (rc < 0) {
			fprintf(logfp, "pthread_join() failed, i %d, rc %d "
					"error %s\n", i, rc, strerror(errno));
			do_exit(1);
		}
	}
}


main(int argc, char *argv[])
{
	int c;
	int i;
	int status;
	pthread_t *tid_list;
	char log_file[256];

	sprintf(log_file, "%s.log", LOG_PREFIX);

	logfp = fopen(log_file, "w");
	if (!logfp) {
		fprintf(stderr, "fopen(%s) failed, %s\n", log_file,
					strerror(errno));
		fflush(stderr);
		do_exit(1);
	}

	if (test_done()) {
		printf("Remove %s before running test\n", TEST_DONE);
		do_exit(1);
	}

	while ((c = getopt(argc, argv, "hd:n:")) != EOF) {
		switch (c) {
		case 'n': num_threads = atoi(optarg); break;
		case 'h':
		default:
			usage(argv);
		}
	};

	for (i=0; i<100; i++) close(i);

	tid_list = create_threads(num_threads);

	/*
	 * Now that we closed the special files and created the threads,
	 * tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	wait_for_threads(tid_list, num_threads);
}
