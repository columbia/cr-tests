/* epoll syscalls */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "libcrtest/libcrtest.h"
#include "libcrtest/labels.h"
#include "libcrtest/log.h"


#define HELLO "Hello world!\n"
