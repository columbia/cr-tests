#include <errno.h>

/*
 * Some of these definitions need to eventually be defined  in system files
 * like <sys/types.h>, <sched.h> etc
 */
#ifndef CLONE_NEWPID
#define CLONE_NEWPID            0x20000000
#endif

#ifndef CLONE_CHILD_SETTID
#define CLONE_CHILD_SETTID      0x01000000
#endif

#ifndef CLONE_PARENT_SETTID
#define CLONE_PARENT_SETTID     0x00100000
#endif

#ifndef CLONE_UNUSED
#define CLONE_UNUSED		0x00001000
#endif

#define STACKSIZE		8192

typedef unsigned long long u64;
typedef unsigned int u32;
typedef int pid_t;

struct clone_args {
        u64 clone_flags_high;
        /*
         * Architectures can use child_stack for either the stack pointer or
         * the base of of stack. If child_stack is used as the stack pointer,
         * child_stack_size must be 0. Otherwise child_stack_size must be
         * set to size of allocated stack.
         */
        u64 child_stack;
        u64 child_stack_size;
        u64 parent_tid_ptr;
        u64 child_tid_ptr;
        u32 nr_pids;
        u32 reserved0;
};

int eclone(int (*fn)(void *), void *fn_arg, int clone_flags_low,
           struct clone_args *clone_args, pid_t *pids);

#if __i386__
#    define __NR_gettid 224
#elif __x86_64__
#    define __NR_gettid	186
#elif __ia64__
#    define __NR_gettid	1105
#elif __s390x__
#    define __NR_gettid	236
#elif __powerpc__
#    define __NR_gettid	207
#else
#    error "Architecture not supported for gettid()"
#endif

/* gettid() is sometimes more useful than getpid() when using clone() */
static inline int gettid()
{
	return syscall(__NR_gettid, 0, 0, 0);
}
