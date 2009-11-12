#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

echo TEST 1: nested user namespaces
bash run_userns.sh
if [ $? -ne 0 ]; then
	echo 'run_userns failed'
	exit 1
fi

echo TEST 2: recreating user task
bash run_usertask.sh
if [ $? -ne 0 ]; then
	echo 'run_usertask failed'
	exit 2
fi

echo TEST 3: securebits
bash run_sbits.sh
if [ $? -ne 0 ]; then
	echo 'run_sbits failed'
	exit 3
fi

echo "TEST 4: simple user_ns hierarchy, and unpriv credentials test"
bash run_simple.sh
if [ $? -ne 0 ]; then
	echo "run_simple failed"
	exit 4
fi

echo "TEST 5: deep user namespaces"
bash run_depth.sh
if [ $? -ne 0 ]; then
	echo "run_depth.sh failed"
	exit 4
fi

echo 'all tests passed'
exit 0
