#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

echo "Running ptree1 test"
sh run-ptree1.sh
ret=$?
if [ $ret -ne 0 ]; then
	echo FAIL: ptree1 returned $ret
	exit 1
fi
echo PASS: ptree1 passed

echo "Running pthread1 test"
sh run-pthread1.sh
ret=$?
if [ $ret -ne 0 ]; then
	echo FAIL: pthread1 returned $ret
	exit 1
fi
echo PASS: pthread1 passed
exit 0
