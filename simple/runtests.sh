#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

RESTART=`which restart`

../ns_exec -cp ./ckpt
if [ ! -f /tmp/cr-test.out ]; then
	echo "FAIL - ckpt did not create /tmp/cr-test.out"
	exit 1
fi
v=`grep ret /tmp/cr-test.out | awk -F=  '{ print $2 '}`
if [ "x$v" == "x" ]; then
	echo "FAIL - ckpt did not put its return value in /tmp/cr-test.out"
	echo "Chances are sys_checkpoint() failed"
	echo "Please see /tmp/cr-test.out for more details"
	exit 2
fi
if [ ! $v -gt 0 ]; then
	echo "FAIL - ckpt return value was $v, should be > 0"
	exit 3
fi

$RESTART --pidns -l rlog -i out
v=`grep ret /tmp/cr-test.out | awk -F=  '{ print $2 }'`
if [ "x$v" == "x" ]; then
	echo "FAIL - restart return value was not in /tmp/cr-test.out"
	exit 4
fi
if [ $v -ne 0 ]; then
	echo "FAIL - restart return value was $v, should be == 0"
	exit 5
fi
echo PASS
exit 0
