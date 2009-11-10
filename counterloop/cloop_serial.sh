#!/bin/bash

# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

source ../common.sh

dir=`mktemp -p . -d -t cr_serial_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"

DEBUG=0
debug()
{
	if [ $DEBUG -eq 1 ]; then
		echo $*
	fi
}

# bash may not be able to wait on the restarted task, so
# here we make sure that we really wait until the restarted
# crcounter has exited.
wait_on_crcounter()
{
	while [ 1 ]; do
		pidof crcounter
		if [ $? -ne 0 ]; then
			break;
		fi
	done
}

# Make sure no stray counter from another run is still going
killall crcounter

./crcounter $dir &
while [ "`cat $dir/counter_out`" == "BAD" ]; do : ; done

NUMLOOPS=50

fail=0
for cnt in `seq 1 $NUMLOOPS`; do
	echo Iteration $cnt
	pid=`pidof crcounter`
	if [  "x$pid" == "x" ]; then
		echo FAIL: crcounter is not running.
		exit 1
	fi
	freeze_pid $pid
	$CHECKPOINT $pid > $dir/o.$cnt
	echo checkpoint returned $?
	kill -9 $pid
	thaw
	wait $pid
	wait_on_crcounter
	echo BAD > $dir/counter_out
	$RESTART --pids < $dir/o.$cnt &
	while [ "`cat $dir/counter_out`" == "BAD" ]; do : ; done
done

if [ $fail -ne 0 ]; then
	echo "WARN there were $fail restart failures"
fi

numjobs=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | wc -l`
killall crcounter

if [ $numjobs -ne 1 ]; then
	echo FAIL - there are $numjobs running, not just 1
	exit 1
fi

echo PASS - one counter is running
exit 0
