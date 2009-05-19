#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

if [ "x$usercrdir" == "x" ]; then
	echo "Please set usercrdir to the location of your user-cr directory"
	exit 1
fi

line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
echo $line | grep "\<ns\>"
if [ $? -eq 0 ]; then
	echo "It looks like ns cgroup is mounted.  Please mount freezer only."
	exit 1
fi

echo Running simple checkpoint/restart test
pushd simple
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 1
fi
popd

echo Running counterloop tests
pushd counterloop
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 2
fi
popd

pushd fileio
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 3
fi
popd

pushd cr-ipc-test
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 4
fi
popd

pushd userns
bash runtest.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 5
fi
popd

exit 0
