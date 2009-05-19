#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

freezermountpoint=/cgroup

CKPT=`which ckpt`
RSTR=`which rstr`

DEBUG=0
my_debug()
{
	if [ $DEBUG -eq 1 ]; then
		echo $*
	fi
}

freeze()
{
	my_debug "freezing $1"
	echo $1 > ${freezermountpoint}/1/tasks
	sleep 0.3s
	echo FROZEN > ${freezermountpoint}/1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
}

unfreeze()
{
	my_debug "unfreezing $1"
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
../ns_exec -m ./crcounter &
sleep 1
pid=`pidof crcounter`
if [  "x$pid" == "x" ]; then
	echo FAIL: crcounter is not running.
	exit 1
fi

freeze $pid
pre=`cat counter_out`
$CKPT $pid > o.1
unfreeze $pid
sleep 7

freeze $pid
prekill=`cat counter_out`
unfreeze $pid
kill $pid

#../ns_exec -m $usercrdir/rstr < ./o.1 &
$RSTR < ./o.1 &
sleep 4
killall crcounter
post=`cat counter_out`

if [ $prekill -le $pre ]; then
	echo FAIL - counter should have incremented in first run
	exit 1
fi
if [ $post -lt $pre ]; then
	echo FAIL - counter should be lower after restart
	exit 1
fi

rm -f o.1

echo PASS
