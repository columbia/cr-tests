#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

dir=`mktemp -p . -d -t cr_sem_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
chmod go+rx $dir

cd $dir

clean_all() {
	rm -f ckpt.sem
	rm -rf sandbox
	killall create-sem
}

do_checkpoint() {
	settimer 2
	while [ ! -f sandbox/sem-created ]; do : ; done
	canceltimer
	pid=`pidof create-sem`
	if [ "x$pid" == "x" ]; then
		echo "failed to execute testcase"
		exit 2
	fi
	freeze_pid $pid
	${CHECKPOINT} $pid > ckpt.sem
	thaw
	killall create-sem
}

echo "XXX Test 1: simple restart with SYSVIPC semaphores"
clean_all
../../ns_exec -ci ../create-sem &
do_checkpoint
# Restart it.  If it finds the sem it created, it creates sem-ok
$RESTART --pids < ckpt.sem
if [ ! -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was not re-created"
	exit 1
fi
echo "PASS"

echo "XXX Test 2: re-create root-owned semaphores as non-root user"
clean_all
../../ns_exec -ci ../create-sem -u 501 &
do_checkpoint
# restart should fail to create sems
$RESTART --pids < ckpt.sem
if [ -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was re-created"
	exit 1
fi
echo "PASS"

# Create semaphores as non-root user
echo "XXX Test 3: create semaphores as non-root user and restart"
clean_all
../../ns_exec -ci ../create-sem -e -u 501 &
do_checkpoint
# restart should be able to create sems
$RESTART --pids  --copy-status < ckpt.sem
if [ ! -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was not re-created"
	exit 1
fi

# can we recreate root ipc objects as non-root user?
clean_all
get_ltp_user
if [ $uid -eq -1 ]; then
	echo "not running ltp-uid test"
	exit 0
fi
../../ns_exec -ci ../create-sem -r -u $uid &
do_checkpoint
chown $uid ckpt.sem
setcap cap_sys_admin+pe $RESTART
cat ckpt.sem | ../../mysu ltp ${RESTART} --pids --copy-status
setcap -r $RESTART
if [ -f sandbox/sem-ok ]; then
	echo "Fail: uid $uid managed to recreate root-owned sems"
	exit 1
fi
echo "PASS: restart failed as it was supposed to"
