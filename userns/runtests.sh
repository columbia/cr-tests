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

echo TEST 1: nested user namespaces
bash run_userns.sh
if [ $? -ne 0 ]; then
	echo 'run_userns failed'
	exit 1
fi

echo TEST 2: recreating user task
bash run_usertask.sh
if [ $? -ne 0 ]; then
	echo 'run_usertask failed'
	exit 2
fi

echo 'all tests passed'
exit 0
