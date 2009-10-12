#!/bin/bash


source ../common.sh

#
# Check if the running kernel supports epoll
#
( if [ -r /proc/config.gz ]; then
	zcat /proc/config.gz
elif [ -r /proc/config ]; then
	cat /proc/config
# elif TODO look for CONFIG_EPOLL=y in /boot/config-$(uname -r)
else
# There is no way to test CONFIG_EPOLL -- assume it is set =y
	echo 'CONFIG_EPOLL=y'
fi ) | grep -E '^[[:space:]]*CONFIG_EPOLL=y' > /dev/null 2>&1
[ $? ] || {
	echo "WARNING: Kernel does not support epoll. Skipping tests."
	exit 1
}

TESTS=( empty pipe )
#make ${TESTS[@]}

# mount -t cgroup foo /cg
# mkdir /cg/1
# chown -R $(id --name -u).$(id --name -g) /cg/1
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
((IMAX = -1))

while [ $CURTEST -lt $NUMTESTS ]; do
	T=${TESTS[$CURTEST]}
	set -x
	if [ "${T}" == "pipe" ]; then
		if (( IMAX < 0 )); then
			((IMAX = $(./${T} -N)))
			((I = 0))
		fi
		TARGS=( "-n" "${I}" )
	else
		TARGS=()
		I=""
	fi
	set +x
	trap 'do_err; break' ERR EXIT
	rm -f ./checkpoint-{ready,done} TBROK
	echo "Running test: ${T}"
	set -x
	./${T} ${TARGS[@]} &
	TEST_PID=$!
	set +x
	while [ '!' -r "./checkpoint-ready" ]; do
		sleep 1
	done
	freeze
	trap 'thaw; do_err; break' ERR EXIT
	sync
	cp log.${T} log.${T}${I}.pre-ckpt
	err_msg="FAIL"
	${CHECKPOINT} ${TEST_PID} > checkpoint-${T}${I}
	err_msg="BROK"
	thaw
	trap 'do_err; break' ERR EXIT
	touch "./checkpoint-done"
	wait ${TEST_PID}
	retval=$?
	echo "Test ${T}${I} done, returned ${retval}"
	if [ -f "TBROK" ]; then
		echo "BROK: epoll snafu, re-running this test"
		continue
	fi
	err_msg="FAIL"
	[ $retval -eq 0 ]
	err_msg="BROK"
	echo PASS

	# now try restarting
	mv log.${T} log.${T}${I}.post-ckpt
	cp log.${T}${I}.pre-ckpt log.${T}
	err_msg="FAIL"
	# --copy-status ensures that we trap on error.
	${RESTART} --copy-status < checkpoint-${T}${I}
	retval=$?
	err_msg="FAIL"
	[ ${retval} -eq 0 ];
	echo PASS
	err_msg="BROK"
	if [ ! -f log.${T}${I} ]; then
		mv log.${T} log.${T}${I}
	fi
	trap '' ERR EXIT

	set -x
	if [ "${T}" == "pipe" ]; then
		((I = I + 1))
		if (( I > IMAX )); then
			((CURTEST = CURTEST + 1))
			((IMAX = -1))
			((I = 0))
		fi
	else
		((CURTEST = CURTEST + 1))
	fi
	set +x
	wait
done
trap '' ERR EXIT

rm -f ./checkpoint-{ready,done}


# rmdir /cg/1
# umount /cg

exit ${failed}
