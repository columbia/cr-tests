#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

source ../common.sh

dir=`mktemp -p . -d -t cr_parallel_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"

DEBUG=0
my_debug()
{
	if [ $DEBUG -eq 1 ]; then
		echo $*
	fi
}

cleanup()
{
	killall crcounter > /dev/null 2>&1
}

do_checkpoint()
{
	pid=$1
	cnt=$2
	# override $freezerdir to the child's private  dir
	oldf=$freezerdir
	freezerdir=$oldf/$cnt
	freeze_pid $pid
	$CHECKPOINT $pid > $dir/d.$cnt/ckpt.out
	thaw
	freezerdir=$oldf
	touch $dir/d.$cnt/ckptdone
}

NUMJOBS=30
checkchildren()
{
	kidsdone=0
	file=$1
	for child in `seq 1 $NUMJOBS`; do
		if [ ! -f $dir/d.$child/$1 ]; then
			my_debug "$dir/d.$child/$1 doesn't exist"
			return
		fi
		if [ "$1" == "ckptdone" ]; then
			continue
		fi
		x=`cat $dir/d.$child/$1`
		if [ "x$x" == "x" ]; then
			my_debug "job $child has empty $dir/d.$child/$1"
			return
		fi
		if [ "$x" == "BAD" ]; then
			my_debug "job $child is still BAD"
			return
		fi
	done
	kidsdone=1
	return
}

cleanup

# We want private freezer cgroups for each child job.  Create
# those now
for i in `seq 1 $NUMJOBS`; do
	mkdir -p $freezerdir/$i
done

echo Starting original set of jobs in parallel
for i in `seq 1 $NUMJOBS`; do
	(mkdir -p $dir/d.$i; cd $dir/d.$i; ../../crcounter )&
done

settimer 5
echo "Waiting for jobs to start..."
kidsdone=0
while [ $kidsdone -eq 0 ]; do checkchildren counter_out; done
echo "... all jobs started"
canceltimer

pids=`ps -ef | grep crcounter | grep -v grep | grep -v nsexec | awk -F\  '{ print $2 '}`

cnt=1

numjobs=`echo $pids | wc -w`

if [ $numjobs -ne $NUMJOBS ]; then
	echo "FAIL: only $numjobs out of $NUMJOBS original jobs started"
	exit 1
fi

echo "Checkpointing all jobs"
for pid in $pids; do
	do_checkpoint $pid $cnt &
	cnt=$((cnt+1))
done

settimer 5
echo "Waiting for checkpoints..."
kidsdone=0
while [ $kidsdone -eq 0 ]; do checkchildren ckptdone; done
echo "... checkpoints done"
canceltimer

killall crcounter
for i in `seq 1 $numjobs`; do
	echo BAD > $dir/d.$i/counter_out
done

echo Waiting for all jobs to die
numjobs=1
count=0
while [ $numjobs -ne 0 ]; do
	numjobs=`ps -ef | grep crcounter | grep -v grep | wc -l`
	count=$((count+1))
	if [ $count -gt 20 ]; then
		killall -9 crcounter
	fi
done

echo Restarting all jobs in parallel

for i in `seq 1 $NUMJOBS`; do
	(cd $dir/d.$i; $RESTART --pids --copy-status < ckpt.out) &
done

settimer 10
echo "Waiting for jobs to restart (10 second timeout)"
kidsdone=0
while [ $kidsdone -eq 0 ]; do checkchildren counter_out; done
echo All jobs restarted
canceltimer

numjobs=`ps -ef | grep crcounter | grep -v grep | wc -l`
killall crcounter

if [ $numjobs -ne $NUMJOBS ]; then
	echo FAIL - only $numjobs out of $NUMJOBS tasks restarted
	exit 1
fi

cleanup

echo PASS
exit 0
