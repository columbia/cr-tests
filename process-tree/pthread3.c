#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libcrtest.h>
#define __USE_UNIX98
#include <pthread.h>

#define	ERROR_EXIT	((void *)1)
#define MIN_STACK_SIZE	(64 *1024)
#define LOG_PREFIX	"logs.d/pthread3"

FILE *logfp;
int num_threads = 4;
int *tstatus;
pthread_barrier_t barrier;
pthread_mutex_t dump_lock;
pthread_key_t key;

struct thread_info {
	long tid;
	int concurrency;
	void *specific;
	sigset_t sigmask;
	int sched_policy;
	struct sched_param sched_param;
};

static void usage(char *argv[])
{
	printf("%s [h] [-n num-threads]\n", argv[0]);
	printf("\t <num-threads> # of threads, default 5\n");
	do_exit(1);
}

void set_thread_info(long tnum)
{
	int rc;
	void *specific;

	specific = (void *)pthread_self();

	rc = pthread_setspecific(key, specific);
	if (rc < 0) {
		fprintf(logfp, "%ld: pthread_setspecific() failed, rc %d, "
				"error %s\n", tnum, rc, strerror(errno));
		do_exit(1);
	}

	/*
	 * TODO: Change other fields in tinfo to some non-default value
	 */
}

void get_thread_info(long tnum, struct thread_info *tinfo)
{
	int rc;

	tinfo->tid = pthread_self();
	tinfo->concurrency = pthread_getconcurrency();
	tinfo->specific = pthread_getspecific(key);

	if (tinfo->specific != (void *)tinfo->tid) {
		fprintf(logfp, "%ld: pthread_getspcific(): expected %p, actual "
				"%p\n", tnum, (void *)tinfo->tid,
				tinfo->specific);
		do_exit(1);
	}

	rc = pthread_sigmask(SIG_SETMASK, NULL, &tinfo->sigmask);
	if (rc < 0) {
		fprintf(logfp, "%ld: pthread_sigmask() failed, rc %d, "
				"error %s\n", tnum, rc, strerror(errno));
		do_exit(1);
	}

	rc = pthread_getschedparam(pthread_self(), &tinfo->sched_policy,
				&tinfo->sched_param);
	if (rc < 0) {
		fprintf(logfp, "%ld: pthread_getschedparam() failed, rc %d, "
				"error %s\n", tnum, rc, strerror(errno));
		do_exit(1);
	}
}

void compare_thread_info(int tnum, struct thread_info *exp_tinfo,
		struct thread_info *act_tinfo)
{
	int rc;

	rc = 0;
	if (exp_tinfo->tid != act_tinfo->tid) {
		rc = 1;
		fprintf(logfp, "thread_info.tid miscompare: expected %p, "
				"actual %p\n", (void *)exp_tinfo->tid,
				(void *)act_tinfo->tid);
	}

	if (exp_tinfo->concurrency != act_tinfo->concurrency) {
		rc = 1;
		fprintf(logfp, "thread_info.concurrency miscompare: expected "
				"%d, actual %d\n", exp_tinfo->concurrency,
				act_tinfo->concurrency);
	}

	if (exp_tinfo->specific != act_tinfo->specific) {
		rc = 1;
		fprintf(logfp, "thread_info.specific miscompare: expected "
				"%p, actual %p\n", exp_tinfo->specific,
				act_tinfo->specific);
	}

	if (memcmp(&exp_tinfo->sigmask, &act_tinfo->sigmask, sizeof(sigset_t))) {
		rc = 1;
		fprintf(logfp, "thread_info.sigmask miscompare: \n");
	}

	if (exp_tinfo->sched_policy != act_tinfo->sched_policy) {
		rc = 1;
		fprintf(logfp, "thread_info.sched_policy miscompare: expected "
				"%d, actual %d\n", exp_tinfo->sched_policy,
				act_tinfo->sched_policy);
	}

	if (memcmp(&exp_tinfo->sched_param, &act_tinfo->sched_param,
				sizeof(struct sched_param))) {
		rc = 1;
		fprintf(logfp, "thread_info.sched_param miscompare: expected "
				"priority %d, actual %d\n",
				exp_tinfo->sched_param.sched_priority,
				act_tinfo->sched_param.sched_priority);
	}

	if (rc)
		do_exit(1);
}


void *do_work(void *arg)
{
	long tnum = (long)arg;
	int rc;
	struct thread_info exp_tinfo, act_tinfo;

	memset(&exp_tinfo, 0, sizeof(struct thread_info));
	memset(&act_tinfo, 0, sizeof(struct thread_info));

	set_thread_info(tnum);

	get_thread_info(tnum, &exp_tinfo);

	fprintf(logfp, "%ld: Thread %lu: waiting for checkpoint\n", tnum,
			pthread_self());
	fflush(logfp);

	/*
	 * Inform main-thread we are ready for checkpoint.
	 */
	rc = pthread_barrier_wait(&barrier);
	if (rc != PTHREAD_BARRIER_SERIAL_THREAD && rc != 0) {
		fprintf(logfp, "%ld: pthread_barrier_wait() failed, rc %d, "
				"error %s\n", tnum, rc, strerror(errno));
		do_exit(1);
	}

	/*
	 * Wait for checkpoint/restart.
	 */
	while(!test_done())
		sleep(1);

	/*
	 * Collect attributes after checkpoint/restart.
	 */
	get_thread_info(tnum, &act_tinfo);

	/*
	 * Compare attributes before and after C/R.
	 */
	compare_thread_info(tnum, &exp_tinfo, &act_tinfo);

	fprintf(logfp, "%ld: Thread %lu: exiting, rc 0\n", tnum,
			pthread_self());
	fflush(logfp);

	tstatus[tnum] = 0;
	pthread_exit((void *)&tstatus[tnum]);
}

static void create_key(pthread_key_t *key)
{
	int rc;

	rc = pthread_key_create(key, NULL);
	if (rc < 0) {
		fprintf(logfp, "pthread_key_create() failed, rc %d, error %s\n",
				rc, strerror(errno));
		do_exit(1);
	}
}

pthread_attr_t *alloc_thread_attr()
{
	int rc;
	pthread_attr_t *attr;

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

	return attr;
}

pthread_t *create_threads(int n)
{
	long i;
	int rc;
	pthread_t *tid_list;
	pthread_t tid;
	pthread_attr_t *attr = NULL;

	tid_list = (pthread_t *)malloc(n * sizeof(pthread_t));
	tstatus = malloc(sizeof(int) * n);

	if (!tid_list || !tstatus) {
		fprintf(logfp, "malloc() failed, n %d, error %s\n",
				n, strerror(errno));
		do_exit(1);
	}

	for (i = 0; i < n; i++) {
		attr = alloc_thread_attr();
		if (!attr)
			do_exit(1);

		rc = pthread_create(&tid, attr, do_work, (void *)i);
		if (rc < 0) {
			fprintf(logfp, "pthread_create(): i %ld, rc %d, "
					"error %s\n", i, rc, strerror(errno));
			do_exit(1);
		}

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

int main(int argc, char *argv[])
{
	int c;
	int i;
	int rc;
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

	fprintf(stderr, "Redirecting output to %s\n", log_file);
	fflush(stderr);

	for (i=0; i<100; i++)  {
		if (fileno(logfp) != i)
			close(i);
	}


	/*
	 * Create a barrier which the main-thread can use to determine
	 * when all threads are ready for checkpoint.
	 */
	rc = pthread_barrier_init(&barrier, NULL, num_threads+1);
	if (rc < 0) {
		fprintf(logfp, "pthread_barrier_init() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}

	rc = pthread_mutex_init(&dump_lock, NULL);
	if (rc) {
		fprintf(logfp, "pthread_mutex_init() failed, rc %d, error %s\n",
				rc, strerror(errno));
		do_exit(1);
	}

	create_key(&key);

	tid_list = create_threads(num_threads);

	/*
	 * Wait for everyone to be ready for checkpoint
	 */
	pthread_barrier_wait(&barrier);
	if (rc != PTHREAD_BARRIER_SERIAL_THREAD && rc != 0) {
		fprintf(logfp, "main: pthread_barrier_wait() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}

	/*
	 * Now that we closed the special files and created the threads,
	 * tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	rc = wait_for_threads(tid_list, num_threads);

	fprintf(logfp, "Exiting with status %d\n", rc);

	do_exit(rc);

	/* not reached */
	return 0;
}
