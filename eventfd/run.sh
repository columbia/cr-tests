#!/bin/bash


source ../common.sh

#
# Check if the running kernel supports eventfd
#
( if [ -r /proc/config.gz ]; then
	zcat /proc/config.gz
elif [ -r /proc/config ]; then
	cat /proc/config
# elif TODO look for CONFIG_EVENTFD=y in /boot/config-$(uname -r)
else
# There is no way to test CONFIG_EVENTFD -- assume it is set =y
	echo 'CONFIG_EVENTFD=y'
fi ) | grep -E '^[[:space:]]*CONFIG_EVENTFD=y' > /dev/null 2>&1
[ $? ] || {
	echo "WARNING: Kernel does not support eventfd. Skipping tests."
	exit 1
}

TESTS=( eventfd01 )

#make ${TESTS[@]}

# mount -t cgroup foo /cg
# mkdir /cg/1
# chown -R $(id --name -u).$(id --name -g) /cg/1
err_msg="BROK"
function do_err()
{
       if [ -n "${TEST_PID}" ]; then
               local PIDLIST=( $(ps --ppid ${TEST_PID} -o pid=) ${TEST_PID} )
               kill ${PIDLIST[@]} || kill -9 ${PIDLIST[@]}
       fi
       echo "${err_msg}"
       ((failed++))
       wait
}

failed=0

NUMTESTS=${#TESTS[@]}
for (( CURTEST = 0; CURTEST < NUMTESTS; CURTEST = CURTEST + 1 )); do
	T=${TESTS[$CURTEST]}
	((IMAX = $(./${T} -N)))
	echo "INFO: Test ${T} does:"
	./${T} -D | sed -e 's/^/INFO:/'
	./${T} -L | sed -e 's/^/INFO:/'

	TEST_LABELS=( $(./${T} -L | tail -n '+2' | cut -f 3) )

	# First we run the test taking checkpoints at all the labelled points
	rm -f "./checkpoint-"{ready,done,skip}
	echo "Running test: \"${T}\""
	./${T} &
	TEST_PID=$!

	trap 'do_err; break 2' ERR EXIT
	for ((I = 0; I <= IMAX; I = I + 1)); do
		TLABEL="${TEST_LABELS[$I]}"
		while [ '!' -r "./checkpoint-ready" ]; do
			sleep 0.1 # slow
		done
		echo "Taking checkpoint ${I}: ${TLABEL}."
		freeze
		trap 'thaw; do_err; break 2' ERR EXIT
		sync # slow
		cp log.${T} log.${T}.${I}.${TLABEL}
		err_msg="FAIL"
		${CHECKPOINT} ${TEST_PID} > "checkpoint-${T}.${I}.${TLABEL}"
		err_msg="BROK"

		# Reset for the next iteration
		touch "./checkpoint-done"
		thaw
		trap 'do_err; break 2' ERR EXIT
	done

	sleep 0.1
	while [ '!' -r "./checkpoint-done" ]; do
		touch "./checkpoint-done"
		sleep 0.1
	done

	echo "Done taking checkpoints. Waiting for ${TEST_PID} to finish."

	# FAIL if the status of the test program's original run isn't 0.
	err_msg="FAIL"
	wait ${TEST_PID} || /bin/true # FIXME why can't we wait for TEST_PID ?
	err_msg="BROK"

	echo "Original completed running."

	# Save the original run's log for later comparison.
	mv log.${T} log.${T}.orig

	# Now that the original, non-restart run is complete let's restart
	# each checkpoint and make sure they produce the same results.
	touch "./checkpoint-ready" "./checkpoint-done" "./checkpoint-skip"
	trap 'do_err; break 2' ERR EXIT
	echo "Restarting checkpoints"
	for ((I=0; I <= IMAX; I = I + 1)); do
		TLABEL="${TEST_LABELS[$I]}"

		# now try restarting. restore log first
		cp log.${T}.${I}.${TLABEL} log.${T}
		echo "Restart ${I} ${TLABEL}"
		err_msg="FAIL"
		# --copy-status ensures that we trap on error.
		${RESTART} --copy-status -w < "checkpoint-${T}.${I}.${TLABEL}"
		err_msg="BROK"

		# Now compare the logs. We can strip the thread id differences
		# since all of these tests are single-threaded.
		SEDEXPR='s/^INFO: thread [[:digit:]]\+: //'
		sed -i -e "${SEDEXPR}" "log.${T}.orig"
		sed -i -e "${SEDEXPR}" "log.${T}"

		err_msg="FAIL"
		diff -s -pu "log.${T}.orig" "log.${T}" > "log.${T}.${I}.${TLABEL}.diff"
		err_msg="BROK"
		echo "PASS \"${T}\""
		rm -f "log.${T}.${I}.${TLABEL}"* "log.${T}" "checkpoint-${T}.${I}.${TLABEL}"
	done
	rm -f "log.${T}.orig"

	trap '' ERR EXIT
	wait
done
trap '' ERR EXIT

rm -f "./checkpoint-"{ready,done,skip}


# rmdir /cg/1
# umount /cg

exit ${failed}
