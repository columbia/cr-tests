#!/bin/sh
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

RSTR=`which rstr`

./ckpt > out &
sleep 1
if [ ! -f /tmp/cr-test.out ]; then
	echo FAIL - ckpt did not create /tmp/cr-test.out
	exit 1
fi
v=`grep ret /tmp/cr-test.out | awk -F=  '{ print $2 '}`
if [ ! $v -gt 0 ]; then
	echo FAIL - ckpt return value was $v, should be > 0
	exit 2
fi

$RSTR < out &
sleep 1
v=`grep ret /tmp/cr-test.out | awk -F=  '{ print $2 '}`
if [ $v -ne 0 ]; then
	echo FAIL - rstrt return value was $v, should be == 0
	exit 2
fi
echo PASS
exit 0
