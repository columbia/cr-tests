/*
 * Copyright 2009 IBM Corporation
 * Author: Nathan Lynch <nathanl@austin.ibm.com>
 */
#ifndef _CHECKPOINT_H
#define _CHECKPOINT_H

#include <sys/types.h>

extern int checkpoint(pid_t pid, int fd, unsigned int flags);

extern int restart(int crid, int fd, unsigned int flags);

#endif /* _CHECKPOINT_H */
