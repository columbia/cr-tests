/* epoll syscalls */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"
#include "libcrtest/labels.h"
#include "libcrtest/log.h"

extern FILE *logfp;

/* Non-reentrant!! */
const char * eflags(unsigned int events);

#define HELLO "Hello world!\n"
