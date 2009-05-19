#!/bin/bash

# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

freezermountpoint=/cgroup

DEBUG=0
debug()
{
	if [ $DEBUG -eq 1 ]; then
		echo $*
	fi
}

freeze()
{
	debug "freezing $1"
	echo $1 > ${freezermountpoint}/1/tasks
	sleep 0.3
	echo FROZEN > ${freezermountpoint}/1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
}

unfreeze()
{
	debug "unfreezing $1"
	echo THAWED > ${freezermountpoint}/1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
	echo $1 > ${freezermountpoint}/tasks
}

# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`

# Make sure no stray counter from another run is still going
killall crcounter

rm counter_out
#../ns_exec -m ./crcounter &
./crcounter &
sleep 0.3

NUMLOOPS=50

for cnt in `seq 1 $NUMLOOPS`; do
	echo Iteration $cnt
	pid=`pidof crcounter`
	if [  "x$pid" == "x" ]; then
		echo FAIL: crcounter is not running.
		exit 1
	fi
	freeze $pid
	sleep 0.3s
	$usercrdir/ckpt $pid > o.$cnt
	echo ckpt returned $?
	unfreeze $pid
	kill -9 $pid
	#../ns_exec -m $usercrdir/rstr < ./o.$cnt &
	sleep 1  # why?  Dunno...
	$usercrdir/rstr < ./o.$cnt &
	v=$((cnt%25))
	if [ $v -eq 0 ]; then
		sleep 4
	else
		sleep 1
	fi
done

numjobs=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | wc -l`
killall crcounter
rm -f o.? o.??

if [ $numjobs -ne 1 ]; then
	echo FAIL - there are $numjobs running, not just 1
	exit 1
fi

echo PASS - one counter is running
exit 0
