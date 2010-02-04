#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

declare -i total=0
declare -i passed=0
declare -i numfailed=0
failed=( )

# $1 = status
# $2 testcase
update_totals() {
    local status=$1
    testcase=$2
    total+=1
    if [ $status -eq 0 ] ; then
	passed+=1
    else
	failed[$numfailed]="$testcase"
	numfailed+=1
	echo FAIL
    fi
}

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
update_totals $? simple
popd

echo Running counterloop tests
pushd counterloop
bash runtests.sh
update_totals $? counterloop
popd

echo Running fileio test
pushd fileio
bash runtests.sh
update_totals $? fileio
popd

echo Running futex tests
pushd futex
bash run.sh
update_totals $? futex
popd

echo Running restart block test
pushd sleep
bash runtest.sh
update_totals $? restart
popd

echo Running process-tree tests
pushd process-tree
sh runtests.sh
update_totals $? process-tree
popd

echo Running bash test
pushd bashckpt
bash bash-cr.sh
update_totals $? bash
popd

echo Running ipc tests
pushd ipc
bash runtests.sh
update_totals $? ipc
popd

echo Running userid/namespace test
pushd userns
bash runtests.sh
update_totals $? userid
popd

echo "Running taskfs (cwd/chroot) tests"
pushd taskfs
bash runtest.sh
update_totals $? taskfs
popd

echo $passed out of $total test groups passed.
echo "Failed tests:"
for t in ${failed[@]}; do echo " $t"; done

exit 0
