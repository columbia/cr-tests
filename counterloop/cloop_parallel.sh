#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

freezermountpoint=/cgroup

freeze()
{
	mkdir ${freezermountpoint}/$1
	sleep 0.3
	echo $1 > ${freezermountpoint}/$1/tasks
	sleep 0.3
	echo FROZEN > ${freezermountpoint}/$1/freezer.state
}

unfreeze()
{
	echo THAWED > ${freezermountpoint}/$1/freezer.state
	echo $1 > ${freezermountpoint}/tasks
	sleep 0.3
	rmdir ${freezermountpoint}/$1
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
killall crcounter > /dev/null 2>&1
rm counter_out > /dev/null 2>&1

NUMJOBS=30

echo Starting original set of jobs in parallel
for i in `seq 1 $NUMJOBS`; do
	(mkdir -p d.$i; cd d.$i; ../crcounter )&
done

echo Giving those 5 seconds to start up
sleep 5
pids=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | awk -F\  '{ print $2 '}`

cnt=1

echo pids is $pids
numjobs=`echo $pids | wc -w`
echo "Checkpoint all the tasks ($numjobs of them)"
for pid in $pids; do
	(freeze $pid; sleep 0.3; $usercrdir/ckpt $pid > d.$cnt/ckpt.out; unfreeze $pid) &
	cnt=$((cnt+1))
done

sleep 2
killall crcounter

echo Restarting all jobs in parallel

for i in `seq 1 $NUMJOBS`; do
	(cd d.$i; $usercrdir/mktree < ckpt.out) &
done

echo Giving those jobs some time to restart...

sleep 5

numjobs=`ps -ef | grep crcounter | grep -v grep | wc -l`
killall crcounter

if [ $numjobs -ne $NUMJOBS ]; then
	echo FAIL - only $numjobs out of $NUMJOBS tasks restarted
	exit 1
fi

rm -f o.? o.??
rm -f d.*/counter_out d.*/ckpt.out
rmdir d.? d.??

echo PASS
exit 0
