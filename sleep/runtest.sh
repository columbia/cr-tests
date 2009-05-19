#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer freezer /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`
mkdir $freezermountpoint/1 > /dev/null 2>&1


freeze()
{
	echo FROZEN > ${freezermountpoint}/1/freezer.state
}

thaw()
{
	echo THAWED > ${freezermountpoint}/1/freezer.state
}

killall sleeptest > /dev/null 2>&1

./sleeptest &
sleep 1
freeze
$usercrdir/ckpt `pidof sleeptest` > o.sleep
thaw
killall sleeptest
$usercrdir/rstr < o.sleep &
sleep 0.3
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
sleep 3

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
