#!/bin/bash

set -e

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

TESTS=( ./plain ./robust )

if [ `ulimit -r` -lt 2 ]; then
	echo "WARNING: Priority inheritance test must be able to set at least two realtime priorities. ulimit -r indicates otherwise so skipping pi futex test(s)."
else
	echo "INFO: Priority inheritance tests included."
	TESTS+=( ./pi )
fi

make ${TESTS[@]}

# mount -t cgroup foo /cg
# mkdir /cg/1
# chown -R $(id --name -u).$(id --name -g) /cg/1

for T in ${TESTS[@]} ; do
	trap 'break' ERR EXIT
	rm -f ./checkpoint-*
	echo "Running test: ${T}"
	${T} &
	TEST_PID=$!
	while [ '!' -r "./checkpoint-ready" ]; do
		sleep 1
	done
	# echo FROZEN > /cg/1/freezer.state
	# ckpt
	# echo THAWED > /cg/1/freezer.state
	touch "./checkpoint-done"
	wait ${TEST_PID}
	retval=$?
	echo "Test ${T} done, returned $retval"
	if [ $retval -ne 0 ]; then
		echo FAIL
	else
		echo PASS
	fi
	trap "" ERR EXIT
done

rm -f ./checkpoint-*

# rmdir /cg/1
# umount /cg
