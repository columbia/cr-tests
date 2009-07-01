#!/bin/bash

# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

freezermountpoint=/cgroup

CKPT=`which ckpt`
RSTR=`which rstr`
MKTREE=`which mktree`

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
	while [ `cat $freezermountpoint}/1/freezer.state` != "FROZEN" ]; do
		echo FROZEN > ${freezermountpoint}/1/freezer.state
	done
}

unfreeze()
{
	debug "unfreezing $1"
	echo THAWED > ${freezermountpoint}/1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
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
rm -rf o.*
#../ns_exec -m ./crcounter &
./crcounter &
while [ ! -f counter_out ]; do : ; done

NUMLOOPS=50

fail=0
for cnt in `seq 1 $NUMLOOPS`; do
	echo Iteration $cnt
	pid=`pidof crcounter`
	if [  "x$pid" == "x" ]; then
		echo FAIL: crcounter is not running.
		exit 1
	fi
	freeze $pid
	$CKPT $pid > o.$cnt
	echo ckpt returned $?
	kill -9 $pid
	unfreeze $pid
	#../ns_exec -m $RSTR < ./o.$cnt &
	wait $pid
	rm -f counter_out
	$RSTR < ./o.$cnt &
	while [ ! -f counter_out ]; do : ; done
done

if [ $fail -ne 0 ]; then
	echo "WARN there were $fail restart failures"
fi

numjobs=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | wc -l`
killall crcounter
rm -f o.? o.??

if [ $numjobs -ne 1 ]; then
	echo FAIL - there are $numjobs running, not just 1
	exit 1
fi

echo PASS - one counter is running
exit 0
