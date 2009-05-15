#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

echo Running simple checkpoint/restart test
pushd simple
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 1
fi
popd

echo Running counterloop tests
pushd counterloop
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 2
fi
popd

pushd fileio
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 3
fi
popd

pushd cr-ipc-test
bash runtests.sh
if [ $? -ne 0 ]; then
	echo FAIL
	exit 4
fi
popd

exit 0
