Futexes are synchronization primitives which optimize the non-contended case and
arbitrate the contended case via the kernel. Furthermore, somewhat like undo
lists that manage semaphores when a task exits, the robust futex list helps
clean up futexes on exit. Finally, to ensure better realtime response there are 
priority-inheritance (pi) futexes.

The non-contended plain futex case is uninteresting as it simply involves
atomically incrementing/decrementing a value. Similarly, robust futexes and pi
futexes have uninteresting non-contended cases. Unlike plain futexes, these set
the futex value to be the thread id.

These tests are designed to trigger the contended cases. We can do this
by carefully setting the initial value of plain futexes, by setting
the initial tid for robust futexes, and by waiting on a plain futex before
trying to grab the pi futex.

Not all architectures support robust and priority-inheritance futexes
because they require futex_atomic_cmpxchg_inatomic. Known-good archs:
	x86, x86_64, sparc64, sh, s390, powerpc

The checkpoint/restart portions of this test require kernel versions
2.6.XX or higher.

Log lines begin with "INFO:", "WARN:", "PASS:", or "FAIL:".

"FAIL:" in any log indicates a failure of the test. Failure is propagated
        via exit codes to the main thread which reports failures via its exit
	code.

"INFO:" Usually indicates what step is about to be taken. It often includes
        specific details such as process ids, futex operations, etc.

"WARN:" Is an unusual condition that doesn't indicate an error but
        which the test was designed to avoid.

"PASS:" Indicates that part of the test passed. Check the exit code for
        a summary of PASS/FAIL.
