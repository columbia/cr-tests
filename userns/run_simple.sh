#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

rm -rf sandbox
mkdir sandbox
./simple_deep &
settimer 5
while [ ! -f sandbox/started ]; do : ; done
canceltimer

job=`jobs -p | head -1`
freeze
echo "Checkpointing job $job"
$CHECKPOINT $job > o.simple
thaw
killall simple_deep

echo "Restarting jobs"
$RESTART --pids < o.simple &

touch sandbox/go
touch sandbox/die

echo "Waiting for jobs to restart and complete"
settimer 5
while [ ! -f sandbox/status ]; do : ; done
canceltimer

get_ltp_user
if [ $uid -eq -1 ]; then
	echo "not running ltp-uid test"
	exit 0
fi
echo "Creating checkpoint image as root"
../simple/ckpt > out

echo "Trying to restart that as uid 500.  Should fail"
chown $uid out /tmp/cr-test*
setcap cap_sys_admin+pe $RESTART
cat out | ../mysu ltp $RESTART --pids --copy-status
ret=$?
setcap -r $RESTART
if [ $ret -eq 0 ]; then
	echo "FAIL restart of priv task as nonpriv user succeeded"
	exit 1
fi

echo PASS
exit 0
