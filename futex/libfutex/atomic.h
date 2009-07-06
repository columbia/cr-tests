#ifndef _ASM_GENERIC_ATOMIC_H_
#define _ASM_GENERIC_ATOMIC_H_
/*
 * Implement the Linux Kernel's atomic_t type in userspace based on:
 * http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html
 */

typedef struct {
	volatile int counter;
} atomic_t;

static inline int atomic_read(atomic_t *v)
{
	return v->counter;
}

static inline void atomic_set(atomic_t *v, int val)
{
	v->counter = val;
}

static inline void atomic_inc(atomic_t *v)
{
	__sync_add_and_fetch(&v->counter, 1);
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	return __sync_val_compare_and_swap(&v->counter, old, new);
}
#endif /* _ASM_GENERIC_ATOMIC_H_ */
