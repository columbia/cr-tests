#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
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
int *tstatus;
pthread_barrier_t barrier;
pthread_mutex_t dump_lock;

static void usage(char *argv[])
{
	printf("%s [h] [-n num-threads]\n", argv[0]);
	printf("\t <num-threads> # of threads, default 5\n");
	do_exit(1);
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

#ifndef debug
void dump_attr(char *msg, pthread_attr_t *attr)
{
}
#endif

void get_affinity(int tnum, pthread_attr_t *attr, cpu_set_t *cpu_set)
{
	int rc;

	fprintf(logfp, "sizeof(cpu_set_t) %zu\n", sizeof(cpu_set_t));

	rc = pthread_attr_getaffinity_np(attr, sizeof(cpu_set_t), cpu_set);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getaffin() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}

}

void compare_affinity(int tnum, pthread_attr_t *exp_attr, pthread_attr_t *act_attr)
{
	cpu_set_t exp_cpus, act_cpus;

	get_affinity(tnum, exp_attr, &exp_cpus);
	get_affinity(tnum, act_attr, &act_cpus);

	if (memcmp(&exp_cpus, &act_cpus, sizeof(cpu_set_t))) {
		fprintf(logfp, "cpu set mismatch\n");
		do_exit(1);
	}
}

void get_detachstate(int tnum, pthread_attr_t *attr, int *state)
{
	int rc;

	rc = pthread_attr_getdetachstate(attr, state);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getdetachstate() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
}

void compare_detachstate(int tnum, pthread_attr_t *exp_attr,
		pthread_attr_t *act_attr)
{

	int exp_state, act_state;

	get_detachstate(tnum, exp_attr, &exp_state);
	get_detachstate(tnum, act_attr, &act_state);

	if (exp_state != act_state) {
		fprintf(logfp, "%d: Thread detach state mismatch: expected %d, "
				"actual %d\n", tnum, exp_state, act_state);
		do_exit(1);
	}
}

void get_guardsize(int tnum, pthread_attr_t *attr, size_t *gsize)
{
	int rc;

	rc = pthread_attr_getguardsize(attr, gsize);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getguardsize() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
}

void compare_guardsize(int tnum, pthread_attr_t *exp_attr,
		pthread_attr_t *act_attr)
{
	size_t exp_size, act_size;

	get_guardsize(tnum, exp_attr, &exp_size);
	get_guardsize(tnum, act_attr, &act_size);

	if (exp_size != act_size) {
		fprintf(logfp, "%d: Thread guard size mismatch, expected %zu "
				"actual %zu\n", tnum, exp_size, act_size);
		do_exit(1);
	}
}

void get_inheritsched(int tnum, pthread_attr_t *attr, int *isched)
{
	int rc;

	rc = pthread_attr_getinheritsched(attr, isched);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_inheritsched() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
}

void compare_inheritsched(int tnum, pthread_attr_t *exp_attr,
		pthread_attr_t *act_attr)
{
	int exp_isched, act_isched;

	get_inheritsched(tnum, exp_attr, &exp_isched);
	get_inheritsched(tnum, act_attr, &act_isched);

	if (exp_isched != act_isched) {
		fprintf(logfp, "%d: Thread inherit-sched mismatch, expected %d "
				"actual %d\n", tnum, exp_isched, act_isched);
		do_exit(1);
	}
}

void get_schedparam(int tnum, pthread_attr_t *attr, int *prio)
{
	int rc;
	struct sched_param param;

	rc = pthread_attr_getschedparam(attr, &param);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getschedparam() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
	*prio = param.__sched_priority;
}

void compare_schedparam(int tnum, pthread_attr_t *exp_attr, pthread_attr_t *act_attr)
{
	int exp_prio, act_prio;

	get_schedparam(tnum, exp_attr, &exp_prio);
	get_schedparam(tnum, act_attr, &act_prio);

	if (exp_prio != act_prio) {
		fprintf(logfp, "%d: Thread sched-param mismatch, expected %d "
				"actual %d\n", tnum, exp_prio, act_prio);
		do_exit(1);
	}
}

void get_schedpolicy(int tnum, pthread_attr_t *attr, int *policy)
{
	int rc;

	rc = pthread_attr_getschedpolicy(attr, policy);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getschedpolicy() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
}

void compare_schedpolicy(int tnum, pthread_attr_t *exp_attr,
			pthread_attr_t *act_attr)
{
	int exp_policy, act_policy;

	get_schedpolicy(tnum, exp_attr, &exp_policy);
	get_schedpolicy(tnum, act_attr, &act_policy);

	if (exp_policy != act_policy) {
		fprintf(logfp, "%d: Thread sched-policy mismatch, expected %d "
				"actual %d\n", tnum, exp_policy, act_policy);
		do_exit(1);
	}
}

void get_scope(int tnum, pthread_attr_t *attr, int *scope)
{
	int rc;

	rc = pthread_attr_getscope(attr, scope);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getscope() failed, rc %d, "
				"error %s\n", rc, strerror(errno));
		do_exit(1);
	}
}

void compare_scope(int tnum, pthread_attr_t *exp_attr, pthread_attr_t *act_attr)
{
	int exp_scope, act_scope;

	get_scope(tnum, exp_attr, &exp_scope);
	get_scope(tnum, act_attr, &act_scope);

	if (exp_scope != act_scope) {
		fprintf(logfp, "%d: Thread scope mismatch, expected %d "
				"actual %d\n", tnum, exp_scope, act_scope);
		do_exit(1);
	}
}

int get_stack(pthread_attr_t *attr, void **addrp, size_t *sizep)
{
	int rc;

	rc = pthread_attr_getstack(attr, (void **)addrp, sizep);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_getstackaddr failed, rc %d, %s\n",
					rc, strerror(errno));
		pthread_exit(ERROR_EXIT);
	}

	return 0;
}

void compare_stack(int tnum, pthread_attr_t *exp_attr,
		pthread_attr_t *act_attr)
{
	size_t exp_size, act_size;
	void *exp_addr, *act_addr;

	get_stack(exp_attr, &exp_addr, &exp_size);
	get_stack(act_attr, &act_addr, &act_size);

	if (act_addr != exp_addr || act_size != exp_size) {
		fprintf(logfp, "%d: Expected: (%p, %zu), actual (%p, %zu)\n",
				tnum, exp_addr, exp_size, act_addr, act_size);
		fflush(logfp);
		do_exit(1);
	}
}

void compare_attr(int tnum, pthread_attr_t *exp_attr, pthread_attr_t *act_attr)
{

	dump_attr("Expected attr", exp_attr);
	dump_attr("Actual attr", act_attr);

	/*
	 * We cannot simply memcmp() the exp_attr and act_attr since the
	 * 'struct pthread_attr' contains a pointer to cpuset. This address
	 * will be different even if the cpusets are the same
	 */
	compare_affinity(tnum, exp_attr, act_attr);

	compare_detachstate(tnum, exp_attr, act_attr);

	compare_guardsize(tnum, exp_attr, act_attr);

	compare_inheritsched(tnum, exp_attr, act_attr);

	compare_schedparam(tnum, exp_attr, act_attr);

	compare_schedpolicy(tnum, exp_attr, act_attr);

	compare_scope(tnum, exp_attr, act_attr);

	compare_stack(tnum, exp_attr, act_attr);
}

void *do_work(void *arg)
{
	long tnum = (long)arg;
	int rc;
	pthread_attr_t exp_attr, act_attr;

	fprintf(logfp, "%ld: Thread %lu: waiting for checkpoint\n", tnum,
			pthread_self());
	fflush(logfp);

	memset(&exp_attr, 0, sizeof(pthread_attr_t));
	memset(&act_attr, 0, sizeof(pthread_attr_t));

	/*
	 * Collect attributes before checkpoint/restart.
	 */
	rc = pthread_getattr_np(pthread_self(), &exp_attr);
	if (rc < 0) {
		fprintf(logfp, "pthread_getattr_np failed, rc %d, %s\n", rc,
				strerror(errno));
		pthread_exit(ERROR_EXIT);
	}

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
	rc = pthread_getattr_np(pthread_self(), &act_attr);
	if (rc < 0) {
		fprintf(logfp, "pthread_getattr_np failed, rc %d, %s\n", rc,
				strerror(errno));
		pthread_exit(ERROR_EXIT);
	}

	/*
	 * Compare attributes before and after C/R.
	 */
	compare_attr(tnum, &exp_attr, &act_attr);

	fprintf(logfp, "%ld: Thread %lu: exiting, rc 0\n", tnum,
			pthread_self());
	fflush(logfp);

	tstatus[tnum] = 0;
	pthread_exit((void *)&tstatus[tnum]);
}

void set_stack(pthread_attr_t *attr, int tnum)
{
	int rc, size;
	void *stack;

	size = MIN_STACK_SIZE + (tnum * getpagesize());

	stack = malloc(size);
	if (!stack) {
		fprintf(logfp, "malloc(stack): error %s\n", strerror(errno));
		do_exit(1);
	}

	rc = pthread_attr_setstack(attr, stack, size);
	if (rc < 0) {
		fprintf(logfp, "pthread_attr_setstack(): rc %d error %s\n",
				rc, strerror(errno));
		do_exit(1);
	}
}

/*
 * Modify any attributes for this thread for testing.
 * For now, we only modify the thread-stack.
 */
void set_thread_attrs(pthread_attr_t *attr, int tnum)
{
	set_stack(attr, tnum);

	return;
}

pthread_t *create_threads(int n)
{
	long i;
	int rc;
	pthread_t *tid_list;
	pthread_t tid;
	pthread_attr_t *attr;

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

		set_thread_attrs(attr, i);

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
