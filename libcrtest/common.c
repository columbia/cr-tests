#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include "libcrtest.h"
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>

FILE *logfp;

void do_exit(int status)
{
	if (logfp) {
		fflush(logfp);
		fclose(logfp);
	}
	_exit(status);
}

int test_done(void)
{
	int rc;

	rc = access(TEST_DONE, F_OK);
	if (rc == 0)
		return 1;
	else if (errno == ENOENT)
		return 0;

	fprintf(logfp, "access(%s) failed, %s\n", TEST_DONE, strerror(errno));
	do_exit(1);

	/* not reached */
	return 0;
}

int test_checkpoint_done(void)
{
	int rc;

	rc = access(CKPT_DRY_RUN, F_OK);
	if (rc == 0)
		return 1;
	rc = access(CKPT_DONE, F_OK);
	if (rc == 0)
		return 1;
	else if (errno == ENOENT)
		return 0;

	fprintf(logfp, "access(%s) failed, %s\n", CKPT_DONE, strerror(errno));
	do_exit(1);

	/* not reached */
	return 0;
}

void set_checkpoint_ready()
{
	int fd, rc;

	rc = access(CKPT_DRY_RUN, F_OK);
	if (rc == 0)
		return;
	fd = creat(CKPT_READY, 0666);
	if (fd < 0) {
		fprintf(logfp, "creat(%s) failed, %s\n", CKPT_READY,
				strerror(errno));
		do_exit(1);
	}
	close(fd);
}

/* Signal ready for and await the checkpoint */
void do_ckpt(void)
{
	int rc;

	set_checkpoint_ready();

	rc = access(CKPT_DRY_RUN, F_OK);
	if (rc == 0)
		return;
	else if (errno != ENOENT)
		do_exit(1);

	while (!test_checkpoint_done())
		usleep(10000);
	if (unlink(CKPT_DONE) == -1) {
		/* perror("unlink(\"./checkpoint-done\")"); */
	}
}

void print_exit_status(int pid, int status)
{
	fprintf(logfp, "Pid %d unexpected exit - ", pid);
	if (WIFEXITED(status)) {
		fprintf(logfp, "exit status %d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		fprintf(logfp, "got signal %d\n", WTERMSIG(status));
	} else {
		fprintf(logfp, "stopped/continued ?\n");
	}
}

int do_wait(int num_children)
{
	int rc;
	int n;
	int status;

	n = 0;
	while(1) {
		rc = waitpid(-1, &status, 0);
		if (rc < 0)
			break;

		n++;
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
			print_exit_status(rc, status);
	}

	if (errno != ECHILD) {
		fprintf(logfp, "waitpid(%d) failed, error %s\n",
					rc, strerror(errno));
		return -1;
	}

	if (getpid() == 1 && num_children && n != num_children) {
		fprintf(logfp, "Only %d of %d children exited ?\n",
			n, num_children);
		return n;
	}

	return 0;
}

void do_sync(FILE *fp)
{
	int rc;

	fflush(fp);
	rc = fsync(fileno(fp));
	if (rc && (fp != logfp))
		fprintf(logfp, "ERROR: fsync %s\n", strerror(errno));
}

void close_all_fds(void)
{
	/* Close everything but stdin, stdout, and stderr */
	DIR *proc_self_fd;
	struct dirent *dent;

	proc_self_fd = opendir("/proc/self/fd");
	if (!proc_self_fd) {
		perror("opendir");
		do_exit(1);
	}

	while ((dent = readdir(proc_self_fd)) != NULL) {
		int fd;

		if (sscanf(dent->d_name, "%12d", &fd) != 1)
			continue;
		if ((logfp && fileno(logfp) == fd) || \
		    (dirfd(proc_self_fd) == fd))
			continue;
		close(fd);
	}
	closedir(proc_self_fd);
}

/*
 * Return 0 if data in srcfp matches data in destfp);
 * Return 1 otherwise.
 */
static int data_compare(FILE *srcfp, FILE *destfp)
{
	int num = 0;
	int rc1, rc2;
	int error1, error2;
	struct record srec, drec;

	rewind(srcfp); rewind(destfp);

	while(1) {
		num++;
		memset(&srec, 0, sizeof(srec));
		memset(&drec, 0, sizeof(drec));

		fprintf(logfp, "COMPARE: Record number %d\n", num);
		fflush(logfp);

		rc1 = fread(&srec, sizeof(srec), 1, srcfp);
		error1 = errno;

		rc2 = fread(&drec, sizeof(srec), 1, destfp);
		error2 = errno;

		if (rc1 != rc2) {
			fprintf(logfp, "MISMATCH: Source: (rc %d, errno %d) "
				"Dest (rc %d, errno %d)\n", rc1, error1, rc2,
					error2);
			fflush(logfp);
			return 1;
		}

		if (memcmp(&srec, &drec, sizeof(struct record))) {
			fprintf(logfp, "DATA MISMATCH\n");
			fflush(logfp);
			return 1;
		}
		if (rc1 == 0)
			return 0;
	}
}

void copy_data(char *srcfile, char *destfile)
{
	int rc;
	int num;
	struct record rec;
	FILE *srcfp, *destfp;

	fprintf(logfp, "COPY-DATA, src %s, dest %s\n", srcfile, destfile);

	srcfp = fopen(srcfile, "r");
	if (!srcfp) {
		fprintf(logfp, "fopen(%s) error %s\n", srcfile,
				strerror(errno));
		do_exit(2);
	}

	destfp = fopen(destfile, "w");
	if (!destfp) {
		fprintf(logfp, "fopen(%s) error %s\n", destfile,
				strerror(errno));
		do_exit(2);
	}

	num = 0;
	while(1) {
		memset(&rec, 0, sizeof(rec));
		rc = fread(&rec, sizeof(rec), 1, srcfp);
		if (rc == 0) {
			if (feof(srcfp)) {
				fprintf(logfp, "Copied %d records\n", num);
				do_sync(destfp);
			} else {
				fprintf(logfp, "Rec %d: fread() error %s\n",
						num, strerror(ferror(srcfp)));
			}
			break;
		}

		fprintf(logfp, " %d: Read record %d %s\n", num, rec.id,
				rec.data);
		fflush(logfp);

		if (fwrite(&rec, sizeof(rec), 1, destfp) != 1) {
			fprintf(logfp, "Rec %d: fwrite() error %s\n", num,
					strerror(errno));
			do_exit(2);
		}

		do_sync(destfp);
		num++;
	}

	if (data_compare(srcfp, destfp))
		do_exit(3);

	if (fclose(srcfp) == EOF)
		fprintf(logfp, "ERROR %s closing srcfile\n", strerror(errno));

	if (fclose(destfp) == EOF)
		fprintf(logfp, "ERROR %s closing destfile\n", strerror(errno));

	return;
}

static char *fieldnumber(char *s, int n)
{
	int i = 0;
	char *c = s;
	while (*c != '\0') {
		if (*c == ' ' || *c == '\t') {
			if (i == n) {
				*c = '\0';
				return s;
			}
			i++;
			if (i == n)
				s = c+1;
		}
		c++;
	}
	return NULL;
}

static int mount_entry_has_option(char *entry, char *which)
{
	char *saveptr, *tmp;

	tmp = strtok_r(entry, ",", &saveptr);
	while (tmp && strlen(tmp) != strlen(which) && strcmp(tmp, which))
		tmp = strtok_r(NULL, ",", &saveptr);
	if (!tmp)
		return 0;
	if (strlen(which) != strlen(tmp))
		return 0;
	if (strcmp(tmp, which) != 0)
		return 0;
	return 1;
}

#define MAXPATH 200
#define MAXLINE 400

static char *freezer_mnt;

char *freezer_mountpoint(void)
{
	if (freezer_mnt)
		return freezer_mnt;
	FILE *fmounts = fopen("/proc/mounts", "r");
	char line[MAXLINE];
	if (!fmounts)
		return NULL;
	while (fgets(line, MAXLINE, fmounts)) {
		char *options, *fstype, *mountpoint;
		options = fieldnumber(line, 3);
		fstype = fieldnumber(line, 2);
		mountpoint = fieldnumber(line, 1);
		if (!fstype || !options || !mountpoint) {
			printf("missing fields in /proc/mounts entry\n");
			do_exit(1);
		}
		if (strcmp(fstype, "cgroup"))
			continue;
		if (!mount_entry_has_option(options, "freezer"))
			continue;
		if (mount_entry_has_option(options, "ns")) {
			printf("freezer is composed with ns subsystem.\n");
			do_exit(1);
		}
		/* success */
		freezer_mnt = malloc(strlen(mountpoint)+1);
		strncpy(freezer_mnt, mountpoint, strlen(mountpoint)+1);
		break;
	}

	fclose(fmounts);
	return freezer_mnt;
}

static void create_cgroup(char *grp)
{
	char dirnam[MAXPATH];
	snprintf(dirnam, MAXPATH, "%s/%s", freezer_mountpoint(), grp);
	mkdir(dirnam, 0755);
}

/*
 * move process pid to subsys cgroup grp
 * return 0 on failure, 1 on success
 */
int move_to_cgroup(char *subsys, char *grp, int pid)
{
	char fname[MAXPATH];
	int rc;

	rc = access(CKPT_DRY_RUN, F_OK);
	if (rc == 0)
		return 1;
	if (strcmp(subsys, "freezer"))
		return 0;
	if (!freezer_mountpoint()) {
		printf("freezer cgroup is not mounted.\n");
		do_exit(1);
	}
	create_cgroup(grp);

	snprintf(fname, MAXPATH, "%s/%s/tasks", freezer_mountpoint(), grp);
	FILE *fout = fopen(fname, "w");
	if (!fout) {
		printf("Failed to open freezer taskfile %s\n", fname);
		return 0;
	}
	if (fprintf(fout, "%d\n", pid) <  0) {
		printf("Failed to write pid to taskfile\n");
		fclose(fout);
		return 0;
	}
	fflush(fout);
	fclose(fout);
	return 1;
}

/*
 * Set up an eventfd for communication between parent/child processes
 */
int setup_notification(void)
{
	int efd;

	efd = eventfd(0, 0);
	if (efd < 0) {
		fprintf(logfp, "ERROR: eventfd(): %s\n", strerror(errno));
		do_exit(1);
	}
	return efd;
}

/*
 * Wait on eventfd @efd till the total number of events equals @total.
 */
void wait_for_events(int efd, u64 total)
{
	int n;
	u64 events;
	u64 count = (u64)0;

	do {
		fprintf(logfp, "%d: wait_for_events: fd %d, reading for %llu\n",
				getpid(), efd, total);
		fflush(logfp);

		n = read(efd, &events, sizeof(events));
		if (n != sizeof(events)) {
			fprintf(logfp, "ERROR: read(event_fd) %s\n",
						strerror(errno));
			do_exit(1);
		}
		fprintf(logfp, "%d: wait_for_events: fd %d read %llu\n",
				getpid(), efd, events);

		count += events;
	} while (count < total);
}

/*
 * Notify one event on the eventfd @efd.
 */
void notify_one_event(int efd)
{
	int n;
	u64 event = (u64)1;

	fprintf(logfp, "%d: Notifying one event on fd %d\n", getpid(), efd);
	fflush(logfp);

	n = write(efd, &event, sizeof(event));
	if (n != sizeof(event)) {
		fprintf(logfp, "ERROR: write(event_fd) %s\n", strerror(errno));
		do_exit(1);
	}
}
