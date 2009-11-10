#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

dir=`mktemp -p . -d -t cr_mq_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
chmod go+rx $dir

cd $dir

clean_all() {
	rm -f ckpt.msq
	rm -rf sandbox
	killall check-mq
}

do_checkpoint() {
	settimer 2
	while [ ! -f sandbox/mq-created ]; do : ; done
	canceltimer
	pid=`pidof check-mq`
	if [ "x$pid" == "x" ]; then
		echo "failed to execute testcase"
		exit 2
	fi
	freeze_pid $pid
	${CHECKPOINT} $pid > ckpt.msq
	thaw
	killall check-mq
}

echo "XXX Test 1: simple restart with SYSVIPC msq"
clean_all
../../ns_exec -ci ../check-mq &
do_checkpoint
# Restart it.  If it finds the msq it created, it creates msq-ok
$RESTART --pids < ckpt.msq
if [ ! -f sandbox/msq-ok ]; then
	echo "Fail: sysv msq was not re-created"
	exit 1
fi
echo "PASS"

echo "XXX Test 2: re-create root-owned msq as non-root user"
clean_all
../../ns_exec -ci ../check-mq -u 501 &
do_checkpoint
# restart should fail to create msq
$RESTART --pids --copy-status < ckpt.msq
if [ -f sandbox/msq-ok ]; then
	echo "Fail: sysv msq was re-created"
	exit 1
fi
echo "PASS"

# Create msq as non-root user
echo "XXX Test 3: create msq as non-root user and restart"
clean_all
../../ns_exec -ci ../check-mq -e -u 501 &
do_checkpoint
# restart should be able to create msq
$RESTART --pids --copy-status < ckpt.msq
if [ ! -f sandbox/msq-ok ]; then
	echo "Fail: sysv msq was not re-created"
	exit 1
fi
echo "PASS"
