#include <stdio.h>
#include <unistd.h>
#include <wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <libcrtest.h>

/*
 * Create a simple process tree and have each process in the tree
 * (except the main) make a private copy of a given input * file,
 * When the copy is done, compare the private copy with the input
 * file and fail if they differ. If not, truncate the private copy
 * and repeat the copying until test_done() is TRUE.
 *
 * Note: To avoid touching restart-blocks, don't sleep().
 */

enum work_type {
	WORK_FILEIO,
	WORK_SLEEP,
};

int max_depth = 2;
int num_children = 2;
char *src = "input.data";
int work = WORK_FILEIO;

#define LOG_PREFIX	"logs.d/ptree1"
#define DATA_PREFIX	"data.d/ptree1"

static void do_work(char *id_str)
{
	char dest[1024];
	int i;

	sprintf(dest, "%s-%s.data", DATA_PREFIX, id_str);

	i = 0;
	while(!test_done()) {
		fprintf(logfp, "%s: do_work() i %d\n", id_str, i++);

		switch (work)  {
		case WORK_FILEIO:
			copy_data(src, dest);
			break;
		case WORK_SLEEP:
			sleep(1);
			break;
		default:
			fprintf(logfp, "Unknow work %d\n", work);
			do_exit(1);
		}
	}

	do_exit(0);
}

static void do_child(int depth, char *id_str);

void create_children(int depth, char *parent_id)
{
	int i;
	int i_len = 16;		// bytes needed to represent 'i' as string
	int child_pid;
	char *child_id;

	child_id = (char *)malloc(strlen(parent_id) + i_len);
	if (!child_id) {
		fprintf(logfp, "malloc() failed depth %d, parent_id %s, "
			"error %s\n", depth, parent_id, strerror(errno));
		do_exit(1);
	}

	for (i = 0; i < num_children; i++) {
		sprintf(child_id, "%s-%d", parent_id, i);

		child_pid = fork();
		if (child_pid == 0)
			do_child(depth, child_id);
		else if (child_pid < 0) {
			fprintf(logfp, "fork() failed, depth %d, "
				"child %d, error %s\n", depth, i,
				strerror(errno));
			do_exit(1);
		}
	}
}

void do_child(int depth, char *id_str)
{
	int i;
	FILE *cfp;
	char cfile[256];
	char *mode = "w";

	/*
	 * Recursively calls do_child() and both parent and child
	 * execute the code below
	 */
	fprintf(logfp, "do_child: depth %d, max_depth %d\n", depth, max_depth);
	fflush(logfp);

	if (depth < max_depth)
		create_children(depth+1, id_str);

	sprintf(cfile, "%s-%s.log", LOG_PREFIX, id_str);

	i = 0;
	while (!test_done()) {
		/* truncate the first time, append after that */
		cfp = fopen(cfile, mode);
		mode = "a";
		if (!cfp) {
			fprintf(logfp, "fopen(%s) failed, error %s\n", cfile,
					strerror(errno));
			do_exit(1);
		}
		fprintf(cfp, "pid %d: i %d\n", getpid(), i++);
		fflush(cfp);

		do_work(id_str);

		fflush(cfp);
		fclose(cfp);
	}

	/* Wait for any children that pre-deceased us */
	do_wait(num_children);

	do_exit(0);
}

static void usage(char *argv[])
{
	printf("%s [h] [-d max-depth] [-n max-children] [-w <sleep|fileio>\n",
				argv[0]);
	printf("\t <max-depth> max depth of process tree, default 3\n");
	printf("\t <num-children> # of children per process, default 3\n");
	do_exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int i;
	char *id_str = "0";
	char log_file[256];

	for (i=0; i<100; i++) close(i);

	sprintf(log_file, "%s-%s.log", LOG_PREFIX, id_str);

	logfp = fopen(log_file, "w");
	if (!logfp) {
		fprintf(stderr, "fopen(%s) failed, %s\n", log_file,
					strerror(errno));
		fflush(stderr);
		do_exit(1);
	}

	if (test_done()) {
		printf("Remove %s before running test\n", TEST_DONE);
		do_exit(1);
	}

	work = WORK_FILEIO;
	while ((c = getopt(argc, argv, "hd:n:w:")) != EOF) {
		switch (c) {
		case 'd': max_depth = atoi(optarg); break;
		case 'n': num_children = atoi(optarg); break;
		case 'w': if (optarg[0] == 's') work = WORK_SLEEP; break;
		case 'h':
		default:
			usage(argv);
		}
	};

	create_children(1, id_str);

	/*
	 * Now that we closed the special files and created process tree
	 * tell any wrapper scripts, we are ready for checkpoint
	 */
	set_checkpoint_ready();

	do_wait(num_children);

	return 0;
}
