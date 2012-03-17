#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn


# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`
if [ ! -d ${freezermountpoint}/1 ]; then
	mkdir ${freezermountpoint}/1
fi

echo TEST 1: correctness of single checkpoint
bash cloop_once.sh
if [ $? -ne 0 ]; then
	echo 'cloop_once failed'
	exit 1
fi

echo TEST 2: serial checkpoints
bash cloop_serial.sh
if [ $? -ne 0 ]; then
	echo 'serial checkpoint test failed'
	exit 2
fi

echo TEST 3: parallel checkpoints
bash cloop_parallel.sh
if [ $? -ne 0 ]; then
	echo 'parallel checkpoint test failed'
	exit 3
fi

echo 'all tests passed'
exit 0
