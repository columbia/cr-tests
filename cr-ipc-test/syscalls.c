/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

/* #include "checkpoint.h" */
#include "../cr.h"

int checkpoint(pid_t pid, int fd, unsigned int flags)
{
	return syscall(__NR_checkpoint, pid, fd, flags);
}

int restart(int crid, int fd, unsigned int flags)
{
	return syscall(__NR_restart, crid, fd, flags);
}
