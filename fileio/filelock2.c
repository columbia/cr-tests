#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include "libcrtest.h"

#define TEST_FILE	"data.d/data.filelock2"
#define LOG_FILE	"logs.d/log.filelock2"

extern FILE *logfp;
int test_fd;
int event_fd1;
int event_fd2;
int mandatory_locks = 1;

/*
 * Description:
 * 	Ensure that a process waiting for a range lock on a file the time
 * 	of checkpoint is properly notified after restart from the checkpoint.
 *
 * Implementation:
 * 	Process P1 acquires a F_WRLCK on a range and waits for checkpoint.
 * 	Process P2 waits in F_SETLKW. After the checkpoint, P1 confirms
 * 	that it still has the lock and then unlocks the range. P2 must
 * 	return succesfully from the fcntl() and must acquire the lock.
 */
struct test_record {
	int start;
	int len;
};

struct test_record test_record = { 0, 17 };

void set_lock(int fd, int lock_type, struct test_record *rec)
{
	int rc;
	struct flock lock;

	lock.l_type = lock_type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (off_t)rec->start;
	lock.l_len = (off_t)rec->len;

	rc = fcntl(fd, F_SETLKW, &lock);
	if (rc < 0 && errno != EAGAIN) {
		fprintf(logfp, "%d: set_lock(): ERROR [%d, %llu, %llu]: %s\n",
				getpid(), lock_type, (u64)rec->start,
				(u64)rec->len, strerror(errno));
		if (mandatory_locks)
			fprintf(logfp, "\n\t***** Is the FS mounted with "
					"'-o mand' option ?\n\n");
		fflush(logfp);
		kill(getppid(), SIGUSR1);
		do_exit(1);
	}

	fprintf(logfp, "%d: set_lock(): [%d, %llu, %llu] %s\n", getpid(),
			lock_type, (u64)rec->start, (u64)rec->len,
			rc < 0 ? strerror(errno) : "done");
}
/*
 * If @set is TRUE, ensure that the given lock is set.
 * If @set is FALSE, ensure that the given lock is NOT set.
 */
void test_lock(int fd, int lock_type, struct test_record *rec)
{
	int rc;
	int conflict;
	struct flock lock;
	char lock_info[512];

	lock.l_type = lock_type;
	lock.l_whence = SEEK_SET;
	lock.l_start = (off_t)rec->start;
	lock.l_len = (off_t)rec->len;
	lock.l_pid = 0;

	sprintf(lock_info, "lock [%d, %llu, %llu] ", lock_type,
			(u64)rec->start, (u64)rec->len);

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

	fprintf(logfp, "%d: %s, conflict %d\n", getpid(), lock_info, conflict);

	if (conflict) {
		fprintf(logfp, "%d: FAIL: %s is NOT set by me !!!\n", getpid(),
				lock_info);
		goto error;
	} else {
		fprintf(logfp, "%d: PASS: %s is set by me\n", getpid(),
				lock_info);
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
	 * We completed the test and sibling completed its test. Safe to
	 * exit.
	 */
	fprintf(logfp, "%d: Ok to exit...\n", getpid());
	fflush(logfp);
	do_exit(0);
}

/*
 * Notify parent that we are done testing and wait for a SIGINT to
 * exit cleanly. Parent will wait for sibling to also exit and then
 * signal us. This orderly exit will help parent distinguish this
 * exit from an unexpected exit by the children.
 */
void exit_cleanly(void)
{
        notify_one_event(event_fd2);
        pause();
	do_exit(0);
}

void do_child1(int idx)
{
	fprintf(logfp, "%d: Child %d starting up\n", getpid(), idx);
	fflush(logfp);

	signal(SIGINT, handler);

	set_lock(test_fd, F_WRLCK, &test_record);

	/*
	 * Tell parent we are ready for checkpoint...
	 */
	notify_one_event(event_fd1);

	/*
	 * Wait for checkpoint/restart
	 */
	fprintf(logfp, "%d: waiting for test-done\n", getpid());
	fflush(logfp);
	while(!test_done()) {
		sleep(1);
	}
	fprintf(logfp, "%d: Found test-done\n", getpid());
	fflush(logfp);

	test_lock(test_fd, F_WRLCK, &test_record);

	/*
 	 * Drop our lock and exit. Sibling can then acquire the lock.
	 */
	set_lock(test_fd, F_UNLCK, &test_record);

	exit_cleanly();
}

void do_child2(int idx)
{
	fprintf(logfp, "%d: Child %d starting up\n", getpid(), idx);
	fflush(logfp);

	signal(SIGINT, handler);

	/*
	 * Tell parent we are about to get the lock.
	 * NOTE: There is still a window between now and when we actually
	 * 	 block in the F_SETLK. But the parent has to guess and sleep()
	 * 	 before enabling checkpoint.
	 */
	notify_one_event(event_fd1);

	/* This should block, until checkpoint/restart is done */
	set_lock(test_fd, F_WRLCK, &test_record);

	/*
	 * We must got here after checkpoint/restart. If not, something is
	 * wrong ?
	 */
	if (!test_checkpoint_done()) {
		fprintf(logfp, "Child2: ERROR: expected C/R to be done "
				"by now, but is not ?\n");
		do_exit(1);
	}

	/*
	 * Since we get here after checkpoint/restart, ensure we have the lock.
	 */
	test_lock(test_fd, F_WRLCK, &test_record);

	exit_cleanly();
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

void kill_children(int sig)
{
	signal(SIGCHLD, SIG_DFL);
	if (pid1)
		kill(pid1, sig);

	if (pid2)
		kill(pid2, sig);
	do_wait(2);
}

/*
 * We get a SIGCHLD if a child exited prematurely. SIGUSR1 if child
 * exited due to an error.
 */
void child_handler(int sig)
{
	fprintf(logfp, "%d: Got signal %d. Test case FAILED\n", getpid(), sig);
	fflush(logfp);

	/*
	 * Kill (remaining) children and exit.
	 */
	kill_children(SIGKILL);

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

int create_child(int idx, void (*child_func)(int))
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

	wait_for_events(event_fd1, 1);

	return rc;
}

int main(int argc, char *argv[])
{
	int i, c;

	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("open() logfile");
		do_exit(1);
	}

	fprintf(logfp, "%d: Parent starting up\n", getpid());
	fflush(logfp);

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

	for (i=0; i<100; i++)  {
		if (fileno(logfp) != i)
			close(i);
	}

	setup_test_file();
	event_fd1 = setup_notification();
	event_fd2 = setup_notification();

	/*
	 * Before waiting for events below, ensure we will be notified
	 * if a child encounters an error and/or exits prematurely.
	 */
	signal(SIGCHLD, child_handler);
	signal(SIGUSR1, child_handler);

	/*
	 * Create the first child and wait for it take its record lock.
	 */
	pid1 = create_child(0, do_child1);

	/*
	 * Create the second child and wait for it to block on the
	 * record lock.
	 *
	 */
	pid2 = create_child(1, do_child2);

	/*
	 * NOTE: We have some guessing to do here. The notification from
	 * 	 the second child (in create_child()) just tells us that
	 * 	 the child is _about_ to attempt the lock. Give it extra
	 * 	 time to actually block before enabling checkpoint.
	 */
	sleep(10);

	/*
	 * Now that the test processes are ready, tell any wrapper scripts,
	 * we are ready for checkpoint.
	 */
	set_checkpoint_ready();

	fprintf(logfp, "***** %d: Ready for checkpoint\n", getpid());
	fflush(logfp);

	/*
	 * Wait for tests to finish their testing.
	 */
	wait_for_events(event_fd2, 2);

	/*
	 * Now tell them that it is safe to exit.
	 *
	 * NOTE: This orderly exit is needed to:
	 *
	 * 	- ensure we don't get stuck waiting for events, and,
	 * 	- enable us to distinguish normal and unexpected exits so
	 * 	  we can properly report test status to wrapper scripts.
	 */
	kill_children(SIGINT);

	do_exit(0);

	/* not reached */
	return 0;
}
