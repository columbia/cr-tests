#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

bash test-sem.sh
if [ $? -ne 0 ]; then
	echo semaphores c/r test failed
	exit  1
fi

bash test-shm.sh
if [ $? -ne 0 ]; then
	echo shm c/r test failed
	exit  1
fi

bash test-mq.sh
if [ $? -ne 0 ]; then
	echo mq c/r test failed
	exit  1
fi

echo 'all tests passed'
exit 0
