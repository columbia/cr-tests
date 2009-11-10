#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

which checkpoint > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo Please place ckpt from user-cr in your PATH
	exit 1
fi
which restart > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo Please place rstr from user-cr in your PATH
	exit 1
fi

export CHECKPOINT=`which checkpoint`
export RESTART=`which restart`

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

echo Running fileio test
pushd fileio
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 3
fi
popd

echo Running futex tests
pushd futex
bash run.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 8
fi
popd

exit 0

echo Running userid/namespace test
pushd userns
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 4
fi
popd

echo Running bash test
pushd bashckpt
bash bash-cr.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 5
fi
popd

echo Running ipc tests
pushd ipc
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 6
fi
popd

echo Running restart block test
pushd sleep
bash runtest.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 6
fi
popd

echo Running process-tree tests
pushd process-tree
sh runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 7
fi
popd

exit 0
