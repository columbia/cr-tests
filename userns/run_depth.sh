#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

dir=`mktemp -p . -d -t cr_depth_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using directory $dir"
chmod go+rx $dir
cd $dir

mkdir sandbox
chown 500:500 sandbox
../userns_deep `basename $freezerdir` &
settimer 5
while [ ! -f sandbox/started ]; do : ; done
canceltimer

job=`jobs -p | head -1`
freeze
echo "Checkpointing job $job"
$CHECKPOINT $job > o.deep
thaw
killall userns_deep

echo "Restarting jobs"
$RESTART --pids --copy-status < o.deep &

touch sandbox/go
touch sandbox/die

echo "Waiting for jobs to restart and complete"
settimer 5
while [ ! -f sandbox/status ]; do : ; done
canceltimer

echo "Verifying uid"
uid=`tail -1 sandbox/status | awk '{ print $2 '}`
if [ $uid -ne 500 ]; then
	echo "FAIL: child was wrong uid ($uid instead of 500)"
	exit 1
fi

echo PASS
exit 0
