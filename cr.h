#if __i386__
#define __NR_checkpoint 335
#define __NR_restart    336
#elif __s390x__
#define __NR_checkpoint 330
#define __NR_restart    331
#elif __powerpc__
#define __NR_checkpoint 322
#define __NR_restart    323
#endif
