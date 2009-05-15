#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

./checkpoint-self
if [ $? -ne 0 ]; then
	echo FAIL: simple self-checkpoint
	exit 1
fi

./restart-self
if [ $? -ne 0 ]; then
	echo FAIL: simple self-restart
	exit 2
fi

./clone-newipc
if [ $? -ne 0 ]; then
	echo FAIL: cloning isolated ipc namespace
	exit 3
fi

./sysv-sem
if [ $? -ne 0 ]; then
	echo FAIL: c/r of SYSV semaphore not in private ns
	exit 4
fi

./checkpoint-sem
if [ $? -ne 0 ]; then
	echo FAIL: c/r of SYSV semaphore in private ns
	exit 5
fi

./checkpoint-shm
if [ $? -ne 0 ]; then
	echo FAIL: c/r of SYSV shm in private ns
	exit 6
fi

./tst_ipcshm_multi
if [ $? -ne 0 ]; then
	echo FAIL: c/r of nested shm namespaces
	exit 7
fi

echo all IPC tests PASS
exit 0
