#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include "libcrtest.h"

#define TEST_FILE	"data.d/data.filelock1"
#define LOG_FILE	"logs.d/log.filelock1"

extern FILE *logfp;
int test_fd;
int event_fd1;
int event_fd2;
int mandatory_locks = 1;

/*
 * Description:
 * 	Ensure that F_RDLCK and F_WRLCK byte-range locks held by a process at
 * 	the time of checkpoint are properly restored when the process is
 * 	restarted from the checkpoint.
 *
 * Implementation:
 * 	Two processes, P0 and P1 acquire the set of locks described by
 * 	locks_list[] below. Then, they notify the parent that they are ready for
 * 	checkpoint and wait for checkpoint to be done.  When they are restarted
 * 	(i.e when test_done() is TRUE), each process verifies that it has the
 * 	locks it had at the time of checkpoint and that it cannot grab a lock
 * 	held by the other process.
 */

struct test_arg {
	int child_idx;
	int type;
	int start;
	int len;
};

struct test_arg locks_list[] = {
	{ 0, F_WRLCK, 0, 17 },
	{ 1, F_WRLCK, 18, 16 },
	{ 0, F_WRLCK, 35, 27 },
	{ 1, F_WRLCK, 63, 17 },
	{ 0, F_RDLCK, 81, 25 },
	{ 1, F_RDLCK, 81, 25 },
};

void set_lock(int fd, struct test_arg *tlock)
{
	int rc;
	struct flock lock;

	lock.l_type = tlock->type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (off_t)tlock->start;
	lock.l_len = (off_t)tlock->len;

	rc = fcntl(fd, F_SETLK, &lock);
	if (rc < 0 && errno != EAGAIN) {
		fprintf(logfp, "%d: set_lock(): ERROR [%d, %llu, %llu]: %s\n",
				getpid(), tlock->type, (u64)tlock->start,
				(u64)tlock->len, strerror(errno));
		if (mandatory_locks)
			fprintf(logfp, "\n\t***** Is the FS mounted with "
					"'-o mand' option ?\n\n");
		fflush(logfp);
		kill(getppid(), SIGUSR1);
		do_exit(1);
	}

	fprintf(logfp, "%d: set_lock(): [%d, %llu, %llu] %s\n", getpid(),
			tlock->type, (u64)tlock->start, (u64)tlock->len,
			rc < 0 ? strerror(errno) : "done");
}
/*
 * If @set is TRUE, ensure that the given lock is set.
 * If @set is FALSE, ensure that the given lock is NOT set.
 */
void test_lock(int fd, int locked_by_me, struct test_arg *tlock)
{
	int rc;
	int conflict;
	struct flock lock;
	char lock_info[512];

	lock.l_type = tlock->type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (off_t)tlock->start;
	lock.l_len = (off_t)tlock->len;
	lock.l_pid = 0;

	sprintf(lock_info, "lock [%d, %llu, %llu] ", tlock->type,
			(u64)tlock->start, (u64)tlock->len);

	conflict = 0;
	rc = fcntl(fd, F_SETLK, &lock);
	if (rc < 0 && (errno == EAGAIN || errno == EACCES)) {
		rc = fcntl(fd, F_GETLK, &lock);
		if (rc < 0) {
			fprintf(logfp, "ERROR: fcntl(F_GETLK): %s, error %s\n",
					lock_info, strerror(errno));
			goto error;
		}

		if (lock.l_type == F_UNLCK || lock.l_pid == 0) {
			fprintf(logfp, "%d: ERROR: %s F_SETLK / F_GETLK "
					"mismatch !!!\n", getpid(), lock_info);
			goto error;
		}
		conflict = 1;
	} else if (rc < 0) {
		fprintf(logfp, "ERROR: fcntl(F_SETLK): %s, error %s\n",
				lock_info, strerror(errno));
		if (mandatory_locks)
			fprintf(logfp, "\n\t***** Is the FS mounted with "
					"'-o mand' option ?\n\n");
		goto error;
	}

	fprintf(logfp, "%d: %s, locked_by_me: %d, conflict %d\n", getpid(),
			lock_info, locked_by_me, conflict);

	if (locked_by_me && conflict) {
		fprintf(logfp, "%d: FAIL: %s is NOT set by me !!!\n", getpid(),
				lock_info);
		goto error;
	} else if (!locked_by_me && !conflict) {
		fprintf(logfp, "%d: FAIL: %s is NOT set by peer !!!\n",
				getpid(), lock_info);
		goto error;
	} else {
		fprintf(logfp, "%d: PASS: %s is %sset by me\n",
				getpid(), lock_info, conflict ? "not " : "");
		return;
	}

error:
	fflush(logfp);
	kill(getppid(), SIGUSR1);
	do_exit(1);
}

void handler(int sig)
{
	/*
	 * We completed the test and siblings have completed their test.
	 * So, safe to drop our locks and exit.
	 */
	fprintf(logfp, "%d: Ok to exit...\n", getpid());
	fflush(logfp);
	do_exit(0);
}

int do_child1(int idx)
{
	int locked_by_me;
	int i;
	int num_locks;

	signal(SIGINT, handler);

	num_locks = sizeof(locks_list) / sizeof(struct test_arg);

	for (i = 0; i < num_locks; i++) {
		if (idx != locks_list[i].child_idx)
			continue;

		set_lock(test_fd, &locks_list[i]);
	}

	/*
	 * Tell parent we are ready for checkpoint...
	 */
	notify_one_event(event_fd1);

	/*
	 * Wait for checkpoint/restart
	 */
	fprintf(logfp, "%d: waiting for test-done\n", idx);
	fflush(logfp);
	while(!test_done()) {
		sleep(1);
	}
	fprintf(logfp, "%d: Found test-done\n", idx);
	fflush(logfp);

	for (i = 0; i < num_locks; i++) {
		/*
		 * If we had (not) set the lock earlier, ensure we still have
		 * it (not) set.
		 */
		locked_by_me = 0;
		if (idx == locks_list[i].child_idx ||
					locks_list[i].type == F_RDLCK)
			locked_by_me = 1;

		test_lock(test_fd, locked_by_me, &locks_list[i]);
	}

	/*
	 * Notify parent that we are done testing the locks.
	 */
	notify_one_event(event_fd2);

	/*
	 * Hold onto our locks and wait for siblings to complete their
	 * test on our locks. Parent will SIGINT us when it is safe to
	 * exit.
	 */
	pause();

	do_exit(0);

	/* not reached */
	return 0;
}

/*
 * Populate the test file so the children can lock some portions of
 * the file
 */
void setup_test_file()
{
	char buf[256];
	int mode;
	int rc;

	mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH; /* 0666 */

	test_fd = open(TEST_FILE, O_RDWR|O_CREAT|O_TRUNC, mode);
	if (test_fd < 0) {
		fprintf(logfp, "ERROR: open(%s): %s\n", TEST_FILE,
				strerror(errno));
		do_exit(1);
	}

	memset(buf, 0, sizeof(buf));
	write(test_fd, buf, sizeof(buf));

	if (!mandatory_locks)
		return;

	/* Enable mandatory file locks (setgid, clear group execute) */
	mode |= S_ISGID;
	mode &= ~S_IXGRP;

	rc = fchmod(test_fd, mode);
	if (rc < 0) {
		fprintf(logfp, "ERROR: fchmod(%s): rc %d, error %s\n",
				TEST_FILE, rc, strerror(errno));
		fprintf(logfp, "Maybe '-o mand' mount option is not set ?\n");
		do_exit(1);
	}
	fprintf(logfp, "Mandatory locking set on %s, mode 0%o\n", TEST_FILE,
			mode);
}

int pid1, pid2;
void child_handler(int sig)
{
	/*
	 * Wait for the child that exited prematurely
	 */
	fprintf(logfp, "%d: Got signal %d\n", getpid(), sig);
	fflush(logfp);

	if (sig == SIGCHLD)
		do_wait(1);

	fprintf(logfp, "%d: Test case FAILED\n", getpid());
	fflush(logfp);
	/*
	 * Kill (remaining) children and exit.
	 */
	if (pid1)
		kill(pid1, SIGKILL);
	if (pid2)
		kill(pid2, SIGKILL);

	do_exit(-1);
}

void usage(char *argv[])
{
	fprintf(logfp, "Usage: %s [-m]\n", argv[0]);
	fprintf(logfp, "\tTest POSIX (advisory) file locks (without -m)\n");
	fprintf(logfp, "\t-m: Test mandatory file locks\n");
	fprintf(logfp, "Test FAILED\n");
	do_exit(1);
}

int main(int argc, char *argv[])
{
	int c;

	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("open() logfile");
		do_exit(1);
	}

	mandatory_locks = 0;
	while((c = getopt(argc, argv, "m")) != EOF) {
		switch (c) {
			case 'm': mandatory_locks = 1; break;
			default: usage(argv);
		}
	}

	if (test_done()) {
		printf("Remove %s before running test\n", TEST_DONE);
		do_exit(1);
	}

	printf("%s: Closing stdio fds and writing messages to %s\n",
			argv[0], LOG_FILE);
	close_all_fds();

	setup_test_file();
	event_fd1 = setup_notification();
	event_fd2 = setup_notification();

	/*
	 * Before waiting for events below, ensure we will be notified
	 * if a child encounters an error and/or exits prematurely.
	 */
	signal(SIGUSR1, child_handler);
	signal(SIGCHLD, child_handler);

	/*
	 * Create the first child and wait for it take its record locks
	 */
	pid1 = fork();
	if (pid1 == 0)
		do_child1(0);
	wait_for_events(event_fd1, 1);

	/*
	 * Create the second child and wait for it take its locks.
	 */
	pid2 = fork();
	if (pid2 == 0)
		do_child1(1);
	wait_for_events(event_fd1, 1);

	/*
	 * Now that the test processes are ready, tell any wrapper scripts,
	 * we are ready for checkpoint
	 */
	set_checkpoint_ready();

	fprintf(logfp, "***** %d: Ready for checkpoint\n", getpid());
	fflush(logfp);

	/*
	 * Wait for all children to test the locks. Since a processes locks
	 * are dropped on exit, if process P1 exits before process P2 has
	 * completed testing a conflicting lock, P2 may acquire the lock
	 * supposed to be held by P1 and wrongly assume that test failed.
	 */
	wait_for_events(event_fd2, 2);

	signal(SIGCHLD, SIG_IGN);

	/*
	 * Tell children it is safe to exit
	 */
	kill(pid1, SIGINT);
	kill(pid2, SIGINT);

	do_wait(2);

	do_exit(0);

	/* not reached */
	return 0;
}
