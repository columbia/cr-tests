#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include "common.h"

void do_exit(int status)
{
	if (logfp) {
		fflush(logfp);
		fclose(logfp);
	}
	_Exit(status);
}

int test_done()
{
	int rc;

	rc = access(TEST_DONE, F_OK);
	if (rc == 0)
		return 1;
	else if (errno == ENOENT)
		return 0;

	fprintf(logfp, "access(%s) failed, %s\n", TEST_DONE, strerror(errno));
	do_exit(1);
}

int test_checkpoint_done()
{
	int rc;

	rc = access(CKPT_DONE, F_OK);
	if (rc == 0)
		return 1;
	else if (errno == ENOENT)
		return 0;

	fprintf(logfp, "access(%s) failed, %s\n", CKPT_DONE, strerror(errno));
	do_exit(1);
}

void set_checkpoint_ready()
{
	int fd;

	fd = creat(CKPT_READY, 0666, 0);
	if (fd < 0) {
		fprintf(logfp, "creat(%s) failed, %s\n", CKPT_READY,
				strerror(errno));
		do_exit(1);
	}
	close(fd);
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

static void do_sync(FILE *fp)
{
	int rc;

	fflush(fp);
	rc = fsync(fileno(fp));
	if (rc && (fp != logfp))
		fprintf(logfp, "ERROR: fsync %s\n", strerror(errno));
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
	int fd;
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
