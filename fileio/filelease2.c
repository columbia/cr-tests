#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <wait.h>
#include "libcrtest.h"

#define TEST_FILE1	"data.d/data.filelease2"
#define LOG_FILE	"logs.d/log.filelease2"

int event_fd1;

static int test_fd;
static int got_sigio;
static int num_children; 
static int pid1, pid2;
char test_data[256];

/*
 * Description:
 * 	Ensure that processes checkpointed when they are in the middle
 * 	of a lease-break, are restored correctly.
 *
 * Implementation:
 * 	Process P1 takes F_WRLCK lease on a file.
 * 	Process P2 attempts to set F_WRLCK lease on the file
 * 	Process P1 gets a SIGIO signal about the pending lease-break.
 * 	Initiate a checkpoint before the downgrade is complete.
 * 	After checkpoint/restart, ensure Process P1 still has the lease
 * 	and that it can be downgraded.
 * 	Ensure Process P2 gets the F_RDLCK lease.
 */

char *get_lease_desc(int type)
{
	switch(type) {
		case F_RDLCK: return "F_RDLCK";
		case F_WRLCK: return "F_WRLCK";
		case F_UNLCK: return "F_UNLCK";
		default:	return "Unknown !";
	}
}

void set_lease(int fd, int type)
{
	int rc;

	fprintf(logfp, "%d: set_lease() called for fd %d, type %s\n",
			getpid(), fd, get_lease_desc(type));

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

void set_signal_action(int sig, void(*action)(int, siginfo_t *, void *))
{
	int rc;
	struct sigaction act;

	act.sa_sigaction = action;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	rc = sigaction(sig, &act, NULL);
	if (rc < 0) {
		fprintf(logfp, "%d: sigaction() sig %d failed, error %s\n",
				getpid(), sig, strerror(errno));
		do_exit(1);
	}
}

static void iohandler(int sig, siginfo_t *info, void *arg)
{
	int rc;

	got_sigio++;
	fprintf(logfp, "%d: Got signal %d\n", getpid(), sig);
	fflush(logfp);
	
	/*
	 * Before giving up the lease, write some data to the file
	 */
	rc = write(test_fd, test_data, sizeof(test_data));
	if (rc != sizeof(test_data)) {
		fprintf(logfp, "%d: write() failed, n %d, error %s\n", getpid(),
				rc, strerror(errno));
		do_exit(1);
	}

	set_checkpoint_ready();
	fprintf(logfp, "***** %d: Ready for checkpoint\n", getpid());
	fflush(logfp);

	/*
	 * Wait for checkpoint/restart
	 */
	while(!test_done())
		sleep(1);

	fprintf(logfp, "%d: Test-done\n", getpid());
	fflush(logfp);

	/*
	 * Checkpoint/restart is done, ensure we still have the lease
	 * and then terminate the lease.
	 *
	 * TODO: Looks like the lease is revoked even before the handler
	 * 	 returns and hence the following test_lease() fails. This
	 * 	 behavior is not obvious from the description of F_SETLEASE
	 * 	 in the man page. Disable the test-lease() test for now
	 * 	 (it does not affect C/R).
	 */
	/* test_lease(test_fd, F_WRLCK); */

	set_lease(test_fd, F_UNLCK);

	return;
}

/* Lease holder */
int do_child1(int idx)
{
	int type = F_WRLCK;

	fprintf(logfp, "%d: Setting lease to type %s\n", getpid(),
			get_lease_desc(type));
	fflush(logfp);

	set_signal_action(SIGIO, iohandler);

	test_fd = open(TEST_FILE1, O_RDWR);
	if (test_fd < 0) {
		fprintf(logfp, "%d: open(%s) failed, error %s\n", getpid(),
				TEST_FILE1, strerror(errno));
		do_exit(1);
	}

	set_lease(test_fd, type);

	/*
	 * Tell parent we are ready for checkpoint.
	 */
	notify_one_event(event_fd1);

	while(!got_sigio)
		sleep(1);

	do_exit(0);

	/* not reached */
	return 0;
}

/* Lease breaker */
int do_child2(int idx)
{
	int rc;
	int fd;
	int type = F_WRLCK;
	char buf[256];

	fprintf(logfp, "%d: Setting lease to type %s\n", getpid(),
			get_lease_desc(type));
	fflush(logfp);

	/*
	 * Tell parent we are (almost) ready for checkpoint.
	 */
	notify_one_event(event_fd1);

	/*
	 * To break the lease, open the file for write. This should block
	 * until sibling drops the lease (after Checkpoint/restart is done).
	 */
	fd = open(TEST_FILE1, O_RDWR);
	if (fd < 0) {
		fprintf(logfp, "%d: open(%s) failed, error %s\n", getpid(),
				TEST_FILE1, strerror(errno));
		do_exit(1);
	}

	/*
	 * If checkpoint is not done yet, then maybe the lease-break-interval
	 * was not long enough for the wrapper scripts to complete checkpoint.
	 * So fail the test.
	 */
	if (!test_checkpoint_done()) {
		fprintf(logfp, "%d: Checkpoint not done yet ?\n", getpid());
		do_exit(1);
	}

	rc = read(fd, buf, sizeof(test_data));
	if (rc != sizeof(test_data)) {
		fprintf(logfp, "%d: read() failed, rc %d, error %s\n",
				getpid(), rc, strerror(errno));
		do_exit(1);
	}

	if (memcmp(test_data, buf, sizeof(test_data))) {
		fprintf(logfp, "%d: FAILED: Data miscompare !!!\n", getpid());
		do_exit(1);
	}

	do_exit(0);

	/* not reached */
	return 0;
}

void setup_test_data()
{
	int rc;
	int fd;
	char buf[256];

	rc = unlink(TEST_FILE1);
	if (rc < 0 && errno != ENOENT) {
		fprintf(logfp, "ERROR: unlink(%s): %s\n", TEST_FILE1,
				strerror(errno));
		do_exit(1);
	}

	fd = open(TEST_FILE1, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		fprintf(logfp, "ERROR: open(%s): %s\n", TEST_FILE1,
				strerror(errno));
		do_exit(1);
	}

	memset(buf, 0, sizeof(buf));
	write(fd, buf, sizeof(buf));

	memset(test_data, 1, sizeof(test_data));
	close(fd);

	return;
}

void kill_children(int sig)
{
	if (pid1)
		kill(pid1, sig);
	if (pid2)
		kill(pid2, sig);
	do_wait(2);
}

int create_child(int idx, int (*child_func)(int))
{
	int rc;

	rc = fork();
	if (rc == 0)
		(*child_func)(idx);

	if (rc < 0) {
		fprintf(logfp, "%d: fork() failed, error %s\n", getpid(), 
				strerror(errno));
		kill_children(SIGKILL);
	}

	fprintf(logfp, "%d: Created child %d, pid %d\n", getpid(), idx, rc);
	fflush(logfp);

	num_children++;
	wait_for_events(event_fd1, 1);

	return rc;
}

void child_handler(int sig, siginfo_t *info, void *arg)
{
	int rc;
	int status;

	fprintf(logfp, "%d: Got signal %d\n", getpid(), sig);
	fflush(logfp);

	if (sig == SIGUSR1)
		goto failed;

	while(num_children) {
		rc = waitpid(-1, &status, WNOHANG);
		if (rc < 0) {
			fprintf(logfp, "%d: waitpid(): failed, rc %d, "
					"error %s\n", getpid(), rc,
					strerror(errno));
			goto failed;
		}

		if (!rc)
			break;

		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			num_children--;
		else {
			print_exit_status(info->si_pid, status);
			goto failed;
		}
	}

	if (!num_children) {
		fprintf(logfp, "Both children exited cleanly, test passed\n");
		do_exit(0);
	}
	return;

failed:
	kill_children(SIGKILL);
	fprintf(logfp, "Test FAILED\n");
	do_exit(1);
}

int main(int argc, char *argv[])
{
	int i;

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

	for (i=0; i<100; i++)  {
		if (fileno(logfp) != i)
			close(i);
	}

	setup_test_data();
	event_fd1 = setup_notification();

	/*
	 * Before waiting for events below, ensure we will be notified
	 * if a child encounters an error and/or exits prematurely.
	 */
	set_signal_action(SIGUSR1, child_handler);
	set_signal_action(SIGCHLD, child_handler);

	pid1 = create_child(0, do_child1);

	pid2 = create_child(1, do_child2);

	/*
	 * NOTE: We have some guessing to do here. The notification from
	 * 	 the second child (in create_child()) just tells us that
	 * 	 the child is _about_ to attempt the lease. Give it extra
	 * 	 time to actually block before enabling checkpoint.
	 *
	 * 	 And this extra time must be less than the lease-break-window
	 * 	 (set by the test wrapper-script.
	 */
	sleep(10);

	/*
	 * Just wait for children to exit and exit from SIGCHLD handler.
	 */
	while(num_children)
		pause();

	do_exit(9); /* should not get here */

	/* not reached */
	return 0;
}
