#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn
# Changelog:
#   Mar 23, 2009: rework into cr_tests

freezermountpoint=/cgroup

source ../common.sh

CHECKPOINT=`which checkpoint`
RESTART=`which restart`

cleanup()
{
	killall crcounter > /dev/null 2>&1
	rm -rf d.* 2>&1
}

freeze()
{
	d="${freezermountpoint}/$1"
	mkdir $d
	while [ ! -d $d ]; do : ; done
	echo $1 > $d/tasks
	cat $d/tasks > /dev/null # make sure state is updated
	echo FROZEN > $d/freezer.state
	while [ `cat $d/freezer.state` != "FROZEN" ]; do
		echo FROZEN > $d/freezer.state
	done
}

unfreeze()
{
	echo THAWED > ${freezermountpoint}/$1/freezer.state
	cat ${freezermountpoint}/$1/freezer.state > /dev/null
	echo $1 > ${freezermountpoint}/tasks
	cat ${freezermountpoint}/tasks > /dev/null
	rmdir ${freezermountpoint}/$1
}

do_checkpoint()
{
	pid=$1
	cnt=$2
	freeze $pid
	$CHECKPOINT $pid > d.$cnt/ckpt.out
	unfreeze $pid
	touch d.$cnt/ckptdone
}

NUMJOBS=30
checkchildren()
{
	kidsdone=0
	file=$1
	for child in `seq 1 $NUMJOBS`; do
		if [ ! -f d.$child/$1 ]; then
			echo "d.$child/$1 doesn't exist"
			return
		fi
		if [ "$1" == "ckptdone" ]; then
			continue
		fi
		x=`cat d.$child/$1`
		if [ "x$x" == "x" ]; then
			echo "job $child has empty d.$child/$1"
			return
		fi
		if [ "$x" == "BAD" ]; then
			echo "job $child is still BAD"
			return
		fi
	done
	kidsdone=1
	return
}

cleanup

echo Starting original set of jobs in parallel
for i in `seq 1 $NUMJOBS`; do
	(mkdir -p d.$i; cd d.$i; ../crcounter )&
done

settimer 5
echo "Waiting for jobs to start..."
kidsdone=0
while [ $kidsdone -eq 0 ]; do checkchildren counter_out; done
echo "... all jobs started"
canceltimer

pids=`ps -ef | grep crcounter | grep -v grep | grep -v ns_exec | awk -F\  '{ print $2 '}`

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
	echo BAD > d.$i/counter_out
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
	(cd d.$i; $RESTART --pids < ckpt.out) &
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
rm -f o.? o.??

echo PASS
exit 0
