#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

verify_cwd() {
	dir=$1

	n=`pidof cwdsleep | wc -w`
	if [ $n -ne 1 ]; then
		echo "Fail: $n tasks restarted, not 1"
		exit 1
	fi
	pid=`pidof cwdsleep`
	cwd=`readlink /proc/$pid/cwd`
	root=`readlink /proc/$pid/root`
	if [ $cwd != $dir ]; then
		echo "Fail: cwd is $cwd should be $dir"
		exit 1
	fi
	if [ $root != '/' ]; then
		echo "Fail: root is $root should be /"
		exit 1
	fi
	return 0;
}

# The root testcase has two processes.  The first should have
# root / and cwd $dir.  The second should have root and cwd both
# as $dir
verify_root() {
	dir=$1

	n=`pidof chrootsleep | wc -w`
	if [ $n -ne 2 ]; then
		echo "Fail: $n tasks restarted, not 2"
		exit 1
	fi
	c=0
	for pid in `pgrep chrootsleep | sort`; do
		cwd=`readlink /proc/$pid/cwd`
		root=`readlink /proc/$pid/root`
		if [ $cwd != $dir ]; then
			echo "cwd is $cwd should be $dir"
			exit 1;
		fi
		if [ $c -eq 0 ]; then
			if [ $root != '/' ]; then
				echo "root is $root should be /"
				exit 1;
			fi
		else
			if [ $root != $dir ]; then
				echo "root is $root should be $dir"
				exit 1;
			fi
		fi
		c=$((c+1))
	done
	return 0;
}

verify_container_chroot() {
	dir=$1

	n=`pidof cwdsleep | wc -w`
	if [ $n -ne 1 ]; then
		echo "Fail: $n tasks restarted, not 1"
		# exit 1
		return 0
	fi
	pid=`pidof cwdsleep`
	cwd=`readlink /proc/$pid/cwd`
	root=`readlink /proc/$pid/root`
	if [ $cwd != $dir ]; then
		echo "Fail: cwd is $cwd should be $dir"
		# exit 1
		return 0
	fi
	if [ $root != $dir ]; then
		echo "Fail: root is $root should be $dir"
		# exit 1
		return 0
	fi
	return 0;
}

runtest() {
	cmd=$1
	dir=$2
	freezerdir=$3
	freezer=`basename $freezerdir`

	killall -9 cwdsleep chrootsleep

	rm -f $dir/log.$cmd

	rm -f $dir/ready
	if [ $cmd == "cwd" ]; then
		(cd $dir; ./cwdsleep $freezer) &
		name=cwdsleep
	elif [ $cmd == "containerroot" ]; then
		(chroot $dir /cwdsleep ) &
		name=cwdsleep
	else
		(cd $dir; ./chrootsleep $freezer) &
		name=chrootsleep
	fi

	settimer 5
	while [ ! -f $dir/ready ]; do : ; done
	canceltimer

	job=`pgrep $name | sort | head -1`
	if [ $cmd == "containerroot" ]; then
		# rather than bind-mount /cgroup into chroot,
		# just special-case the cgroup entering
		echo $job >> $freezerdir/tasks
		cat $freezerdir/tasks > /dev/null
	fi

	freeze
	echo "Checkpointing job $job"
	echo $CHECKPOINT -l $dir/log.$cmd $job > $dir/ckpt.$cmd
	$CHECKPOINT -l $dir/log.$cmd $job > $dir/ckpt.$cmd

	killall -9 cwdsleep chrootsleep
	thaw

	echo "Restarting job"
	$RESTART -W --pids < $dir/ckpt.$cmd &
	settimer 5
	job=`pidof $name`
	ret=$?
	if [ $cmd == containerroot ]; then ret=0; fi
	while [ $ret -ne 0 ]; do
		job=`pidof $name`
		ret=$?
	done
	canceltimer
	if [ $cmd == "cwd" ]; then
		verify_cwd $dir
	elif [ $cmd == "container_chroot" ]; then
		verify_container_chroot $dir
	else # root
		verify_root $dir
	fi
	killall -9 cwdsleep chrootsleep

	echo "Test $cmd PASS"
	return 1
}

source ../common.sh
cwd=`pwd`
dir=`mktemp -p $cwd -d -t cr_taskfs_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
chmod go+rx $dir
cp cwdsleep chrootsleep $dir/

echo "Test 1: testing cwd"
runtest cwd $dir $freezerdir

echo "Test 2: chroot"
runtest root $dir $freezerdir

#echo "Test 3: (expected to fail)"
#runtest containerroot $dir $freezerdir

echo PASS
exit 0
