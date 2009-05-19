#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

which ckpt
ret1=$?
which mktree
ret2=$?
which rstr
ret3=$?
if [ $ret1 -ne 0 || $ret2 -ne 0 || $ret3 -ne 0 ]; then
	echo Please place the ckpt, rstr, and mktree programs from user-cr
	echo in your PATH.
	exit 1
fi

export CKPT=`which ckpt`
export MKTREE=`which mktree`
export RSTR=`which rstr`

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

echo Running userid/namespace test
pushd userns
bash runtest.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 4
fi
popd

echo Running restart block test
pushd sleep
bash runtest.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 5
fi
popd

# these currently fail...
pushd cr-ipc-test
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 6
fi
popd

exit 0
