/*
 * Copyright 2009 IBM Corp.
 * Author: Sukadev Bhattiprolu <sukadev@us.ibm.com>
 */

#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/*
 * Create a 'test-input' input file.
 *
 * Repeat:
 *	1. In new process, copy data from 'test-input' to a 'test-output'
 *	   file, one record at a time.
 *
 *	2. When all records are copied, compare the input file with the output
 *	   file and ensure they are the same.
 *
 *	3. Goto: Repeat;
 *
 *	Checkpoint/restart the process when in step 1 repeatedly.
 */
#define LOG_FILE	"log.fileio1"
#define SLOW_DOWN_FILE 	"slow-down-fileio"
#define CKPT_READY	"checkpoint.ready"
#define COPY_DONE	"copy.done"

char log_fnam[400];
char slowdown_fnam[400];
char ckptready_fnam[400];
char copydone_fnam[400];

void setup_filenames(char *dir)
{
	if (dir) {
		snprintf(log_fnam, 400, "%s/%s", dir, LOG_FILE);
		snprintf(slowdown_fnam, 400, "%s/%s", dir, SLOW_DOWN_FILE);
		snprintf(ckptready_fnam, 400, "%s/%s", dir, CKPT_READY);
		snprintf(copydone_fnam, 400, "%s/%s", dir, COPY_DONE);
	} else {
		snprintf(log_fnam, 400, "%s", LOG_FILE);
		snprintf(slowdown_fnam, 400, "%s", SLOW_DOWN_FILE);
		snprintf(ckptready_fnam, 400, "%s", CKPT_READY);
		snprintf(copydone_fnam, 400, "%s", COPY_DONE);
	}
}

FILE *logfp;

static void usage(char *argv0)
{
	fprintf(logfp, "%s -C <file>\n", argv0);
	fprintf(logfp, "\t create test data file\n");

	fprintf(logfp, "%s -c <file1> <file2>\n", argv0);
	fprintf(logfp, "\t (slowly) copy test data from file1 to file2\n");

	fprintf(logfp, "%s -v <file1> <file2>\n", argv0);
	fprintf(logfp, "\t  Verify that data in file1 and file2 are same\n");

	fflush(logfp);
	_exit(1);
}

/*
 * Use larger number when working with a bigger system than the KVM on T61.
 * NFS performance sucks on that config.
 */
#define NUM_RECS 1024

struct record {
	int id;
	char data[256];
};

static void do_sync(FILE *fp)
{
	int rc;

	fflush(fp);
	rc = fsync(fileno(fp));
	if (rc && (fp != logfp))
		fprintf(logfp, "ERROR: fsync %s\n", strerror(errno));
}

static void do_exit(int status)
{
	unlink(ckptready_fnam);

	do_sync(logfp);
	fclose(logfp);

	_exit(status);
}

static void create_data(char *destfile)
{
	int i;
	struct record rec;
	FILE *destfp;

	fprintf(logfp, "Mode CREATE-DATA, dest %s\n", destfile);

	destfp = fopen(destfile, "w");
	if (!destfp) {
		fprintf(logfp, "fopen(%s) error %s\n", destfile,
				strerror(errno));
		do_exit(2);
	}

	for (i = 0; i < NUM_RECS; i++) {
		rec.id = i;
		memset(rec.data, 0, sizeof(rec.data));
		sprintf(rec.data, "Record number: %d", i);
		if (fwrite(&rec, sizeof(rec), 1, destfp) != 1) {
			fprintf(logfp, "Rec %d: fwrite() error %s\n", i,
					strerror(errno));
			do_exit(2);
		}
	}
	do_sync(destfp);
	do_exit(0);
}

/*
 * If a 'slow-down' file exists, slow-down the operation a bit to give
 * our handlers an oppurtunity to checkpoint/restart us.
 */
static void slow_down(int rec_num)
{
	int rc;
	int num_skip = 10;

	rc = access(slowdown_fnam, F_OK);

	if (rc == 0) {

		/* For records:
		 * 	1..20,  sleep for every other record.
		 *	20..30, sleep after every 2 records
		 *	30..40, sleep after every 3 records etc
		 */
		if (rec_num > 100)
			num_skip = rec_num / 10;

		if (!(rec_num % num_skip))
			sleep(1);
	} else if (errno != ENOENT) {
		fprintf(logfp, "access(%s) error %s\n", slowdown_fnam,
				strerror(errno));
		do_exit(1);
	}
	return;
}

static void copy_data(char *srcfile, char *destfile)
{
	int fd;
	int rc;
	int num;
	struct record rec;
	FILE *srcfp, *destfp;

	fprintf(logfp, "Mode COPY-DATA, src %s, dest %s\n", srcfile, destfile);

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
			/*
			 * Announce we are now done copying
			 */
			fd = creat(copydone_fnam, 0666);
			if (fd < 0) {
				fprintf(logfp, "creat(%s) error %s\n",
						 copydone_fnam, strerror(errno));
				do_exit(1);
			}
			close(fd);
			do_exit(0);
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

		slow_down(num);
	}

	if (fclose(srcfp) == EOF)
		fprintf(logfp, "ERROR %s closing srcfile\n", strerror(errno));

	if (fclose(destfp) == EOF)
		fprintf(logfp, "ERROR %s closing destfile\n", strerror(errno));

	do_exit(0);
}

static void verify_data(char *srcfile, char *destfile)
{
	fprintf(logfp, "Mode VERIFY-DATA, src %s, dest %s\n", srcfile, destfile);
	do_exit(0);
}

enum test_mode {
	CREATE_DATA = 1,
	COPY_DATA,
	VERIFY_DATA
};

int main(int argc, char *argv[])
{
	int c;
	int fd;
	int i;
	char *srcfile;
	char *destfile;
	enum test_mode mode;
	char *mydir = NULL;

	srcfile = NULL;
	destfile = NULL;
	mode = 0;

	if (argc > 2 &&  strcmp(argv[1], "-d") == 0) {
		mydir = argv[2];
		argc -= 2;
		argv += 2;
	}
	setup_filenames(mydir);

	logfp = fopen(log_fnam, "w");
	if (!logfp) {
		perror("logfile");
		_exit(1);
	}

	/*
	 * Cannot checkpoint process with open device files yet;
	 */
	printf("Closing stdio fds and writing messages to %s\n", log_fnam);
	for (i=0; i<100; i++)  {
		if (i != fileno(logfp))
			close(i);
	}

	/*
	 * Announce that we are now prepared for a checkpoint 
	 */
	fd = creat(ckptready_fnam, 0666);
	if (fd < 0) {
		fprintf(logfp, "creat(%s) error %s\n", ckptready_fnam,
				strerror(errno));
		do_exit(1);
	}
	close(fd);

	while ((c = getopt(argc, argv, "C:c:v:")) != EOF) {
		switch (c) {
		case 'C':
			mode = CREATE_DATA; destfile = optarg; break;
		case 'c':
			mode = COPY_DATA; srcfile = optarg; break;
		case 'v':
			mode = VERIFY_DATA; srcfile = optarg; break;
		default:
			usage(argv[0]);
		}
	}

	if (mode < CREATE_DATA || mode > VERIFY_DATA)
		usage(argv[0]);

	if (mode == COPY_DATA || mode == VERIFY_DATA) {

		if (optind == argc) {
			fprintf(logfp, " -c, -v need two files, src, dest\n");
			do_exit(1);
		}

		destfile = argv[optind];
	}

	switch(mode) {
	case CREATE_DATA:
		create_data(destfile);
	case COPY_DATA:
		copy_data(srcfile, destfile);
	case VERIFY_DATA:
		verify_data(srcfile, destfile);
	default:
		fprintf(logfp, "Invalid mode %d ?\n", mode);
		do_exit(1);
	}
	do_exit(0);
}
