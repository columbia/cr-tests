#!/bin/bash

set -e

source ../common.sh

#
# Check if the running kernel supports futexes
#
if [ -r /proc/config.gz ]; then
	zcat /proc/config.gz | grep -F 'CONFIG_FUTEX=y' > /dev/null 2>&1
	[ $? ] || {
		echo "WARNING: Kernel does not support futexes. Skipping tests."
		exit 1
	}
fi
if [ -r /proc/config ]; then
	cat /proc/config | grep -F 'CONFIG_FUTEX=y' > /dev/null 2>&1
	[ $? ] || {
		echo "WARNING: Kernel does not support futexes. Skipping tests."
		exit 1
	}
fi

TESTS=( plain robust )

if [ `ulimit -r` -lt 2 ]; then
	echo "WARNING: Priority inheritance test must be able to set at least two realtime priorities. ulimit -r indicates otherwise so skipping pi futex test(s)."
else
	echo "INFO: Priority inheritance tests included."
	TESTS+=( pi )
fi

#make ${TESTS[@]}

# mount -t cgroup foo /cg
# mkdir /cg/1
# chown -R $(id --name -u).$(id --name -g) /cg/1

# Most failures indicate a broken test environment
err_msg="BROK"
function do_err()
{
	if [ -n "${TEST_PID}" ]; then
		local PIDLIST=( $(ps --ppid ${TEST_PID} -o pid=) ${TEST_PID} )
		kill ${PIDLIST[@]}
	fi
	echo "${err_msg}"
	((failed++))
	wait
}

failed=0

NUMTESTS=${#TESTS[@]}
CURTEST=0

while [ $CURTEST -lt $NUMTESTS ]; do
	T=${TESTS[$CURTEST]}
	trap 'do_err; break' ERR EXIT
	rm -f ./checkpoint-* TBROK
	echo "Running test: ${T}"
	./${T} &
	TEST_PID=$!
	while [ '!' -r "./checkpoint-ready" ]; do
		sleep 1
	done
	freeze
	trap 'thaw; do_err; break' ERR EXIT
	sync
	cp log.${T} log.${T}.pre-ckpt
	err_msg="FAIL"
	ckpt ${TEST_PID} > checkpoint-${T}
	err_msg="BROK"
	thaw
	trap 'do_err; break' ERR EXIT
	touch "./checkpoint-done"
	wait ${TEST_PID}
	retval=$?
	echo "Test ${T} done, returned $retval"
	if [ -f "TBROK" ]; then
		echo "BROK: Futex snafu, re-running this test"
		continue
	fi
	err_msg="FAIL"
	[ $retval -eq 0 ]
	err_msg="BROK"
	echo PASS

	# now try restarting
	mv log.${T} log.${T}.post-ckpt
	cp log.${T}.pre-ckpt log.${T}
	err_msg="FAIL"
	# We need to pass -p to mktree since futexes often store the
	# pid of the task that owns the futex in the futex, even in
	# the uncontended cases where the kernel is entirely unaware
	# of the futex. --copy-status ensures that we trap on error.
	${MKTREE} -p --copy-status < checkpoint-${T}
	retval=$?
	err_msg="BROK"
	mv log.${T} log.${T}.post-rstr
	# Now we can do something _like_:
	#         diff log.${T}.post-ckpt log.${T}.post-rstr
	echo "Restart of test ${T} done, returned $retval"
	err_msg="FAIL"
	[ $retval -eq 0 ];
	echo PASS
	trap '' ERR EXIT
	CURTEST=$((CURTEST+1))

	# Wait for restarted tasks to complete.
	wait
done
trap '' ERR EXIT

rm -f ./checkpoint-{ready,done}

# rmdir /cg/1
# umount /cg

exit ${failed}
