/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

void pass(void)
{
	exit(0);
}

static void do_msg(const char *msg)
{
	if (errno)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	fflush(stderr);
}

void fail(const char *msg)
{
	do_msg(msg);
	exit(1);
}

void bail(const char *msg)
{
	do_msg(msg);
	exit(2);
}

