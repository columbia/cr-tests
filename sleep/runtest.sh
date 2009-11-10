#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

dir=`mktemp -p . -d -t cr_sleep_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"

killall sleeptest > /dev/null 2>&1

cd $dir
../sleeptest `basename $freezerdir` &
sleep 1
freeze
$CHECKPOINT `pidof sleeptest` > o.sleep
thaw
killall sleeptest
$RESTART --pids < o.sleep &
sleep 1
pidof sleeptest
if [ $? -ne 0 ]; then
	echo FAIL: restart failed entirely
	exit 1
fi

# sleeptest sleeps twice for 3 secs each
# we wasted 1+ second before frezing and checkpoint it,
# then another 0.3 after restart
# 3 seconds definately should bring us into the second
# 3-second sleep
sleep 1

pidof sleeptest
if [ $? -ne 0 ]; then
	echo FAIL: restart block not set up right - first sleep not resumed
	exit 1
fi

sleep 3

pidof sleeptest
if [ $? -eq 0 ]; then
	echo FAIL: restart block not set up right - sleep never ended
	killall sleeptest
	exit 1
fi

echo PASS: restart blocks were set up correctly
