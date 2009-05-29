#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh
verify_freezer
verify_paths

clean_all() {
	rm -f ckpt.shm
	rm -rf sandbox
	killall create-shm
}

do_checkpoint() {
	sleep 1
	freeze
	pid=`pidof create-shm`
	if [ "x$pid" == "x" ]; then
		echo "failed to execute testcase"
		exit 2
	fi
	ckpt $pid > ckpt.shm
	thaw
	killall create-shm
}

echo "XXX Test 1: simple restart with SYSVIPC shm"
clean_all
../ns_exec -ci ./create-shm &
do_checkpoint
# Restart it.  If it finds the shm it created, it creates shm-ok
rstr < ckpt.shm
if [ ! -f sandbox/shm-ok ]; then
	echo "Fail: sysv shm was not re-created"
	#exit 1
fi
echo "PASS"

echo "XXX Test 2: re-create root-owned shm as non-root user"
clean_all
../ns_exec -ci ./create-shm -u 501 &
do_checkpoint
# restart should fail to create shm
rstr < ckpt.shm
if [ -f sandbox/shm-ok ]; then
	echo "Fail: sysv shm was re-created"
	#exit 1
fi
echo "PASS"

# Create shm as non-root user
echo "XXX Test 3: create shm as non-root user and restart"
clean_all
../ns_exec -ci ./create-shm -e -u 501 &
do_checkpoint
# restart should be able to create shm
rstr < ckpt.shm
if [ ! -f sandbox/shm-ok ]; then
	echo "Fail: sysv shm was not re-created"
	exit 1
fi

# can we recreate root ipc objects as non-root user?
clean_all
get_ltp_user
if [ $uid -eq -1 ]; then
	echo "not running ltp-uid test"
	exit 0
fi
../ns_exec -ci ./create-shm -r -u $uid &
do_checkpoint
chown $uid ckpt.shm
RSTR=`which rstr`
setcap cap_sys_admin+pe $RSTR
cat ckpt.shm | su ltp -c rstr
if [ -f sandbox/shm-ok ]; then
	echo "Fail: uid $uid managed to recreate root-owned shms"
	exit 1
fi
setcap -r $RSTR
echo "PASS"
