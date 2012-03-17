#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include "libcrtest.h"

#define LOG_FILE	"logs.d/log.fsetown1"

int pipe_fds[2];
int event_fd1;
int got_sigio;

/*
 * Description:
 * 	Checkpoint a process that is waiting for async notification of data
 * 	being available on a pipe. When the process is restarted, make data
 * 	available on the pipe and ensure that the process is notified.
 *
 * Implementation:
 */
void iohandler(int sig)
{
	fprintf(logfp, "%d: Got signal %d\n", getpid(), sig);
	fflush(logfp);
	got_sigio = 1;
}

static void wait_for_child(void)
{
	int rc;
	int status;

	rc = waitpid(-1, &status, 0);
	if (rc < 0) {
		fprintf(logfp, "%d: waitpid(): rc %d, error %s\n",
				getpid(), rc, strerror(errno));
		do_exit(1);
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		fprintf(logfp, "%d: Test case PASSED\n", getpid());
		rc = 0;
	} else {
		fprintf(logfp, "%d: Test case FAILED\n", getpid());
		print_exit_status(rc, status);
		rc = 1;
	}
	do_exit(rc);
}

void set_owner(int fd)
{
	int rc;
	long flags;

	fprintf(logfp, "%d: Setting owner to myself\n", getpid());

	signal(SIGIO, iohandler);

	flags = O_ASYNC;
	rc = fcntl(fd, F_SETFL, flags);
	if (rc < 0) {
		fprintf(logfp, "%d: set_owner(): F_SETFL ERROR %s\n", getpid(),
				strerror(errno));
		goto error;
	}

	rc = fcntl(fd, F_SETOWN, getpid());
	if (rc < 0) {
		fprintf(logfp, "%d: set_owner():, ERROR %s\n", getpid(),
				strerror(errno));
		if (errno == EINVAL)
			fprintf(logfp, "%d: Maybe the fs does not support "
					"F_SETLEASE (eg: NFS)\n", getpid());
		goto error;
	}

	fprintf(logfp, "%d: Set owner() done\n", getpid());
	return;

error:
	/*
	 * Parent will be waiting for notification. Signal that we failed
	 * and are exiting
	 */
	kill(getppid(), SIGUSR1);
	do_exit(1);
}

/*
 * Called by parent to see if child is still the owner
 */
void test_owner(int fd, int exp_owner)
{
	int rc;

	rc = fcntl(fd, F_GETOWN, 0);
	if (rc < 0) {
		fprintf(logfp, "%d: ERROR: fcntl(F_GETOWN) error %s\n",
				getpid(), strerror(errno));
		do_exit(1);
	}

	if (rc != exp_owner) {
		fprintf(logfp, "%d: FAILED: Expected owner %d, actual %d\n",
				getpid(), exp_owner, rc);
		/*
		 * Terminate the child since it will not be notified of I/O.
		 */
		kill(exp_owner, SIGKILL);
		wait_for_child();
		do_exit(1);
	}

	fprintf(logfp, "%d: PASS: Owner is %d\n", getpid(), exp_owner);
	return;
}

void do_child()
{
	int rc;
	char buf[16];
	int fd = pipe_fds[0];

	set_owner(fd);

	/*
	 * Tell parent we are ready for checkpoint...
	 */
	notify_one_event(event_fd1);

	/*
	 * Read data from the pipe. If this synchronous read finds data
	 * without a SIGIO signal, then we were not notified and the
	 * test fails.
	 */
	fprintf(logfp, "%d: Waiting for data to be available\n", getpid());
	fflush(logfp);

	rc = read(fd, buf, 4);
	if (rc <= 0) {
		fprintf(logfp, "%d: ERROR: read(): rc %d, error %s\n",
				getpid(), rc, strerror(errno));
		do_exit(1);
	} else if (!got_sigio) {
		fprintf(logfp, "%d: FAILED: read() found data but did not"
				"get SIGIO, rc %d buf %.4s\n", getpid(),
				rc, buf);
		do_exit(1);
	} else {
		fprintf(logfp, "%d: PASS: Got SIGIO, read data, rc %d, "
				"buf '%.4s'\n", getpid(), rc, buf);
		do_exit(0);
	}
}

/*
 * Create a pipe that the child will try to read from and parent will
 * write to.
 */
void setup_test_data(void)
{
	int rc;

	rc = pipe(pipe_fds);
	if (rc < 0) {
		fprintf(logfp, "%d: pipe() failed, rc %d, error %s\n",
				getpid(), rc, strerror(errno));
		do_exit(1);
	}

	return;
}

void usr1_handler(int sig)
{
	/*
	 * Test failed or a child encountered an error.
	 * Reap the child, report error and exit.
	 */
	fprintf(logfp, "%d: Signal %d, Test case FAILED\n", getpid(), sig);
	fflush(logfp);

	wait_for_child();
}

int main(int argc, char *argv[])
{
	int rc;
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
	 * if a child encounters an error.
	 */
	signal(SIGUSR1, usr1_handler);

	/*
	 * Create the child process and wait for it to be ready for checkpoint.
	 */
	pid = fork();
	if (pid == 0)
		do_child();

	if (pid < 0) {
		fprintf(logfp, "%d: fork() failed, error %s\n", getpid(),
				strerror(errno));
		do_exit(1);
	}

	wait_for_events(event_fd1, 1);

	/*
	 * Tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	fprintf(logfp, "%d: ***** Ready for checkpoint\n", getpid());
	fflush(logfp);

	/* Wait for wrappers to complete checkpoint/restart */
	while(!test_done())
		sleep(1);

	/* Ensure that child is still owner for the read side of pipe */
	test_owner(pipe_fds[0], pid);

	/* Make data available on the pipe for the child */
	rc = write(pipe_fds[1], "done", 4);
	if (rc < 0) {
		fprintf(logfp, "%d: write() failed, rc %d, error %s\n",
				getpid(), rc, strerror(errno));
		kill(pid, SIGKILL);
		do_exit(1);
	}

	fflush(logfp);
	wait_for_child();

	/* not reached, wait_for_child exits. */
	return 0;
}
