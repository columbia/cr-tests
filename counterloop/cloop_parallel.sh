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
	echo FROZEN > ${freezermountpoint}/$1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
}

unfreeze()
{
	debug "unfreezing $1"
	echo THAWED > ${freezermountpoint}/$1/freezer.state
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "failed to freeze, return value $ret"
	fi
}

# Check freezer mount point
line=`grep freezer /proc/mounts`
echo $line | grep "\<ns\>"
if [ $? -ne 0 ]; then
	echo "please mount freezer and ns cgroups"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer,ns cgroup /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`

# Make sure no stray counter from another run is still going
killall crcounter

NUMJOBS=30

rm counter_out
for i in `seq 1 $NUMJOBS`; do
	mkdir -p d.$i
	(cd d.$i; ../../ns_exec -m ../crcounter )&
done
sleep 1
pids=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | awk -F\  '{ print $2 '}`

cnt=1
for pid in $pids; do
	debug freezing $pid
	freeze $pid
	../cr $pid o.$cnt
	unfreeze $pid
	cnt=$((cnt+1))
done

killall crcounter

for i in `seq 1 $NUMJOBS`; do
	(cd d.$i; ../../rstr ../o.$i ) &
done

sleep 1

numjobs=`ps -ef | grep crcounter | grep -v grep | wc -l`
killall crcounter

debug numjobs is $numjobs
if [ $numjobs -ne $NUMJOBS ]; then
	echo FAIL - some jobs failed to restart
	exit 1
fi

killall crcounter

rm -f o.? o.??
rm -f d.*/counter_out
rmdir d.? d.??

echo PASS
exit 0
