#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libcrtest.h>
#include <pthread.h>

int num_threads = 5;
FILE *logfp;
/*
 * Use LOG_PREFIX with thread index as suffix, if each thread needs a
 * separate log file. For now, we use a single log
 */
#define LOG_PREFIX		"logs.d/pthread1"

static void usage(char *argv[])
{
	printf("%s [h] [-n num-threads]\n", argv[0]);
	printf("\t <num-threads> # of threads, default 5\n");
	do_exit(1);
}

struct test_arg {
	pthread_mutex_t *mutex;
	pthread_cond_t  *cond;
};

void *
do_work(void *arg)
{
	int rc;
	struct test_arg *targ = (struct test_arg *)arg;

	fprintf(logfp, "Thread %lu: waiting...\n", pthread_self());
	fflush(logfp);

	rc = pthread_cond_wait(targ->cond, targ->mutex);
	if (rc < 0) {
		perror("pthread_cond_wait()");
		exit(1);
	}
	pthread_mutex_unlock(targ->mutex);

	fprintf(logfp, "Thread %lu: wokeup...\n", pthread_self());
	rc = pthread_cond_signal(targ->cond);
	if (rc < 0)
		fprintf(logfp, "do_work(): pthread_cond_signal() failed %s\n",
					strerror(errno));

	fprintf(logfp, "Thread %lu: exiting...\n", pthread_self());
	fflush(logfp);
}

void *
do_work_coord(void *arg)
{
	int rc;
	struct test_arg *targ = (struct test_arg *)arg;

	while(!test_done())
		sleep(1);

	fprintf(logfp, "Thread %lu: test-done\n", pthread_self());

	rc = pthread_cond_signal(targ->cond);
	if (rc < 0)
		fprintf(logfp, "do_work_coord(): pthread_cond_signal() error "
				"%s\n", strerror(errno));

	fprintf(logfp, "Thread %lu: exiting...\n", pthread_self());
	fflush(logfp);
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
struct test_arg targ;

pthread_t *create_threads(int n)
{
	int i;
	int rc;
	pthread_t *tid_list;
	pthread_t tid;

	targ.mutex = &mutex;
	targ.cond = &cond;

	tid_list = (pthread_t *)malloc(n * sizeof(pthread_t));
	if (!tid_list) {
		fprintf(logfp, "malloc(%d) failed, error %s\n",
				n * sizeof(pthread_t), strerror(errno));
		do_exit(1);
	}

	rc = pthread_create(&tid, NULL, do_work_coord, &targ);
	if (rc < 0) {
		fprintf(logfp, "pthread_create() of coord failed, rc %d "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
	tid_list[0] = tid;

	for (i = 1; i < n; i++) {
		rc = pthread_create(&tid, NULL, do_work, &targ);
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

	fprintf(stderr, "Redirecting output to logfile %s\n", log_file);

	for (i=0; i<100; i++) {
		if (i != fileno(logfp))
			close(i);
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

	tid_list = create_threads(num_threads);

	/*
	 * Now that we closed the special files and created the threads,
	 * tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	wait_for_threads(tid_list, num_threads);
}
