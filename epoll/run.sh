#!/bin/bash


source ../common.sh
dir=`mktemp -p . -d -t cr_epoll_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
cd $dir

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

TESTS=( empty pipe sk10k cycle )
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
	if (( IMAX < 0 )); then
		((IMAX = $(./${T} -N)))
		((I = 0))
		echo "INFO: Test ${T} does:"
		../${T} -h | sed -e 's/^/INFO:/'
	fi
	TLABEL=$(../${T} -L | head -n +$((I + 2)) | tail -n 1 | cut -f 3)
	TARGS=( "-l" "${TLABEL}" )
	trap 'do_err; break' ERR EXIT
	rm -f ./checkpoint-{ready,done} TBROK
	echo "Running test: ${T} -l ${TLABEL}"
	mkdir -p $freezerdir
	../${T} ${TARGS[@]} -f `basename $freezerdir` &
	TEST_PID=$!
	while [ '!' -r "./checkpoint-ready" ]; do
		sleep 1
	done
	freeze
	trap 'thaw; do_err; break' ERR EXIT
	sync
	cp log.${T} log.${T}${LABEL}.pre-ckpt
	err_msg="FAIL"
	${CHECKPOINT} ${TEST_PID} > checkpoint-${T}${LABEL}
	err_msg="BROK"
	thaw
	trap 'do_err; break' ERR EXIT
	touch "./checkpoint-done"
	wait ${TEST_PID}
	retval=$?
	echo "Test ${T}${LABEL} done, returned ${retval}"
	if [ -f "TBROK" ]; then
		echo "BROK: epoll snafu, re-running this test"
		continue
	fi
	err_msg="FAIL"
	[ $retval -eq 0 ]
	err_msg="BROK"
	echo "PASS ${T} ${TLABEL} original"

	# now try restarting
	mv log.${T} log.${T}${LABEL}.post-ckpt
	cp log.${T}${LABEL}.pre-ckpt log.${T}
	err_msg="FAIL"
	# --copy-status ensures that we trap on error.
	${RESTART} --copy-status < checkpoint-${T}${LABEL}
	retval=$?
	err_msg="FAIL"
	[ ${retval} -eq 0 ];
	echo "PASS ${T} ${TLABEL} restart"
	err_msg="BROK"
	if [ ! -f log.${T}${LABEL} ]; then
		mv log.${T} log.${T}${LABEL}
	fi
	trap '' ERR EXIT

	((I = I + 1))
	if (( I > IMAX )); then
		((CURTEST = CURTEST + 1))
		((IMAX = -1))
		((I = 0))
	fi
	wait
done
trap '' ERR EXIT

# TODO add scm testcase to run.sh

# rmdir /cg/1
# umount /cg

exit ${failed}
