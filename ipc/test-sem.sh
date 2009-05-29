#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh
verify_freezer
verify_paths

clean_all() {
	rm -f ckpt.sem
	rm -rf sandbox
	killall create-sem
}

do_checkpoint() {
	sleep 1
	freeze
	pid=`pidof create-sem`
	if [ "x$pid" == "x" ]; then
		echo "failed to execute testcase"
		exit 2
	fi
	ckpt $pid > ckpt.sem
	thaw
	killall create-sem
}

echo "XXX Test 1: simple restart with SYSVIPC semaphores"
clean_all
../ns_exec -ci ./create-sem &
do_checkpoint
# Restart it.  If it finds the sem it created, it creates sem-ok
rstr < ckpt.sem
if [ ! -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was not re-created"
	#exit 1
fi
echo "PASS"

echo "XXX Test 2: re-create root-owned semaphores as non-root user"
clean_all
../ns_exec -ci ./create-sem -u 501 &
do_checkpoint
# restart should fail to create sems
rstr < ckpt.sem
if [ -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was re-created"
	#exit 1
fi
echo "PASS"

# Create semaphores as non-root user
echo "XXX Test 3: create semaphores as non-root user and restart"
clean_all
../ns_exec -ci ./create-sem -e -u 501 &
do_checkpoint
# restart should be able to create sems
rstr < ckpt.sem
if [ ! -f sandbox/sem-ok ]; then
	echo "Fail: sysv sem was not re-created"
	exit 1
fi
echo "PASS"