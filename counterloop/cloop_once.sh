#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

source ../common.sh

dir=`mktemp -p . -d -t cr_once_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"

DEBUG=0
my_debug()
{
	if [ $DEBUG -eq 1 ]; then
		echo $*
	fi
}

# Make sure no stray counter from another run is still going
killall crcounter

echo BAD > $dir/counter_out
../ns_exec -m ./crcounter $dir &
while [ "`cat $dir/counter_out`" == "BAD" ]; do : ; done
pid=`pidof crcounter`

freeze_pid $pid
pre=`cat $dir/counter_out`
$CHECKPOINT $pid > $dir/o.1
thaw

echo "sleeping for 7 seconds to let counter_out be incremented"
sleep 7

freeze_pid $pid
prekill=`cat $dir/counter_out`
thaw
kill $pid

$RESTART --pids < $dir/o.1 &

echo "sleeping for 4 seconds to inc counter_out by less than last time"
sleep 4
killall crcounter
post=`cat $dir/counter_out`

echo prekill is $prekill pre is $pre post is $post
if [ $prekill -le $pre ]; then
	echo FAIL - counter should have incremented in first run
	exit 1
fi
if [ $post -lt $pre ]; then
	echo FAIL - counter should be lower after restart
	exit 1
fi

echo PASS
