#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

rm -rf sandbox pidfile
mkdir sandbox
chown 500:500 sandbox
./userns_ckptme &
settimer 5
while [ ! -f sandbox/started ]; do : ; done
canceltimer

job=`jobs -p|head -1`
freeze
echo "Checkpointing job $job"
$CKPT $job > o.userns
thaw
killall userns_ckptme

echo "Restarting jobs"
$MKTREE < o.userns &

touch sandbox/go
touch sandbox/die

echo "Waiting for jobs to restart and complete"
settimer 3
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
