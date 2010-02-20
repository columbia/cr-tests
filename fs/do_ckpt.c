#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_FILE	"log.do_ckpt"
#include "libfstest.h"

int main(int argc, char** argv)
{
	/* FIXME eventually stdio streams should be harmless */
	close(0);
	logfp = fopen(LOG_FILE, "w");
	if (!logfp) {
		perror("could not open logfile");
		exit(1);
	}
	dup2(fileno(logfp), 1); /* redirect stdout and stderr to the log file */
	dup2(fileno(logfp), 2);
	if (!move_to_cgroup("freezer", "1", getpid())) {
		log_error("move_to_cgroup");
		exit(2);
	}

	do_ckpt();
	exit(EXIT_SUCCESS);
}
