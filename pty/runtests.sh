#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

RSTR=`which rstr`

source ../common.sh
verify_freezer
verify_paths
echo THAWED > /cgroup/1/freezer.state

rm -f read-ok read-bad
./ptyloop &
pid=$!
sleep 1
if [ -f "read-bad" ]; then
	echo "BROK: read failed in original program"
	exit 1
fi
#if [ ! -f "read-ok" ]; then
	#echo "BROK: read did not succeed in original program"
	#exit 1
#fi

freeze
sleep 1
$CKPT $pid > ckpt-out
echo press enter to continue
read x
killall ptyloop
thaw
sleep 0.5

echo "restarting"
rm -f read-ok read-bad
$MKTREE < ckpt-out &
sleep 1
if [ -f "read-bad" ]; then
	echo "FAIL: read was bad"
	exit 1
fi
#if [ ! -f "read-ok" ]; then
	#echo "FAIL: read did not succeed"
	#exit 1
#fi

echo PASS
exit 0
