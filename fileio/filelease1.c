#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "libcrtest.h"

#define TEST_FILE1	"data.d/data.filelease1"
#define TEST_FILE2	"data.d/data.filelease2"
#define LOG_FILE	"logs.d/log.filelease1"

extern FILE *logfp;
int test_fd;
int event_fd1;
int event_fd2;

/*
 * Description:
 * 	Ensure that F_RDLCK and F_WRLCK file leases held by a process at
 * 	the time of checkpoint are properly restored when the process is
 * 	restarted from the checkpoint.
 *
 * Implementation:
 * 	Two processes, P0 and P1 acquire a F_RDLCK lease on file F1.
 * 	Process P2 acquires a F_WRLCK lease on file F2. After acquiring
 * 	leases the processes notify parent they are ready for checkpoint
 * 	and wait for checkpoint to be done.  When they are restarted
 * 	(i.e when test_done() is TRUE), each process verifies that it has the
 * 	lease it had at the time of checkpoint.
 */

void set_lease(int fd, int type)
{
	int rc;

	fprintf(logfp, "%d: set_lease() called for fd %d, type %d\n",
			getpid(), fd, type);

	rc = fcntl(fd, F_SETLEASE, type);
	if (rc < 0) {
		fprintf(logfp, "%d: set_lease(type %d):, ERROR %s\n",
				getpid(), type, strerror(errno));
		if (errno == EINVAL)
			fprintf(logfp, "%d: Maybe the fs does not support "
					"F_SETLEASE (eg: NFS)\n", getpid());
		fflush(logfp);
		kill(getppid(), SIGUSR1);
		do_exit(1);
	}

	fprintf(logfp, "%d: set_lease(%d): %s\n", getpid(), type,
			rc < 0 ? strerror(errno) : "done");
}

char *get_lease_desc(int type)
{
	switch(type) {
		case F_RDLCK: return "F_RDLCK";
		case F_WRLCK: return "F_WRLCK";
		case F_UNLCK: return "F_UNLCK";
		default:	return "Unknown !";
	}
}

void test_lease(int fd, int exp_type)
{
	int rc;

	rc = fcntl(fd, F_GETLEASE, 0);
	if (rc < 0 || rc > 2) {
		fprintf(logfp, "ERROR: fcntl(F_GETLEASE): expected %s, rc %d, "
				"error %s\n", get_lease_desc(exp_type), rc,
				strerror(errno));
		do_exit(1);
	}

	if (rc != exp_type) {
		fprintf(logfp, "%d: FAIL: Expected %s, actual %s\n", getpid(),
				get_lease_desc(exp_type), get_lease_desc(rc));
		do_exit(1);
	}

	fprintf(logfp, "%d: PASS: Expected %s, actual %s\n", getpid(),
			get_lease_desc(exp_type), get_lease_desc(rc));
	return;
}

struct test_arg {
	int fd;
	int type;
	int pid;
};

struct test_arg test_data[3];

void do_child(int idx)
{
	int type = test_data[idx].type;
	int fd = test_data[idx].fd;

	fprintf(logfp, "%d: Setting lease to type %s\n", getpid(),
			get_lease_desc(type));

	set_lease(fd, type);

	/*
	 * Tell parent we are ready for checkpoint...
	 */
	notify_one_event(event_fd1);

	/*
	 * Wait for checkpoint/restart
	 */
	fprintf(logfp, "%d: waiting for test-done\n", getpid());
	while(!test_done()) {
		sleep(1);
	}
	fprintf(logfp, "%d: Found test-done\n", getpid());

	test_lease(fd, type);

	do_exit(0);
}

/*
 * Create two test files and populate test_data[] so that:
 * 	- first two childrent get a F_RDLCK lease on file TEST_FILE1.
 * 	- third child gets a F_WRLCK lease on file TEST_FILE2.
 */
void setup_test_data()
{
	int fd;
	char buf[256];

	/* Create TEST_FILE1 */
	fd = open(TEST_FILE1, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		fprintf(logfp, "ERROR: open(%s): %s\n", TEST_FILE1,
				strerror(errno));
		do_exit(1);
	}

	memset(buf, 0, sizeof(buf));
	write(fd, buf, sizeof(buf));

	/* Close TEST_FILE1 and open for read-only */
	close(fd);

	fd = open(TEST_FILE1, O_RDONLY);
	if (fd < 0) {
		fprintf(logfp, "ERROR: open(%s): %s\n", TEST_FILE1,
				strerror(errno));
		do_exit(1);
	}

	/*
	 * First two childrent get a F_RDLCK lease on file TEST_FILE1.
	 * Third child gets a F_WRLCK lease on file TEST_FILE2.
	 */
	test_data[0].fd = test_data[1].fd = fd;
	test_data[0].type = test_data[1].type = F_RDLCK;
	fprintf(logfp, "fd0: %d, type %d\n",
				test_data[0].fd, test_data[0].type);

	/* Create TEST_FILE2 */
	fd = open(TEST_FILE2, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		fprintf(logfp, "ERROR: open(%s): %s\n", TEST_FILE2,
				strerror(errno));
		do_exit(1);
	}
	write(fd, buf, sizeof(buf));

	test_data[2].fd = fd;
	test_data[2].type = F_WRLCK;

	return;
}

void child_handler(int sig)
{
	int i;
	int num_children = 3;
	/*
	 * Test failed or a child encountered an error.
	 * Kill (remaining) children, reap children and exit.
	 */
	fprintf(logfp, "%d: Got signal %d\n", getpid(), sig);
	for (i = 0; i < num_children; i++)
		if (test_data[i].pid)
			kill(test_data[i].pid, SIGKILL);

	fprintf(logfp, "%d: Test case FAILED\n", getpid());
	fflush(logfp);

	do_wait(num_children);

	do_exit(-1);
}

int main(int argc, char *argv[])
{
	int i;
	int pid;

	if (test_done()) {
		printf("Remove %s before running test\n", TEST_DONE);
		do_exit(1);
	}

	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("open() logfile");
		do_exit(1);
	}

	printf("%s: Closing stdio fds and writing messages to %s\n",
			argv[0], LOG_FILE);
	close_all_fds();

	setup_test_data();
	event_fd1 = setup_notification();

	/*
	 * Before waiting for events below, ensure we will be notified
	 * if a child encounters an error and/or exits prematurely.
	 */
	signal(SIGUSR1, child_handler);
	signal(SIGCHLD, child_handler);

	/*
	 * Create the test processes and wait for them to be ready for
	 * checkpoint.
	 */
	for (i = 0; i < 3; i ++) {
		pid = fork();
		if (pid == 0)
			do_child(i);
		test_data[i].pid = pid;
	}

	wait_for_events(event_fd1, 1);

	/*
	 * Now that the test processes are ready, tell any wrapper scripts,
	 * we are ready for checkpoint
	 */
	set_checkpoint_ready();

	fprintf(logfp, "***** %d: Ready for checkpoint\n", getpid());
	fflush(logfp);

	do_wait(3);

	do_exit(0);

	/* not reached */
	return 0;
}
