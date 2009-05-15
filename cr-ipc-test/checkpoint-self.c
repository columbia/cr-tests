/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
/* Simply attempt a checkpoint.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "checkpoint.h"
#include "util.h"

int main(int argc, char **argv)
{
	FILE *out;
	int rc;

	out = tmpfile();
	if (!out)
		bail("tmpfile");

	/* Checkpointing terminals not supported yet */
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	rc = checkpoint(getpid(), fileno(out), 0);
	if (rc < 0)
		fail("checkpoint");

	return 0;
}
