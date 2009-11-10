#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

RESTART=`which restart`

dir=`mktemp -p . -d -t cr_simple_XXXXXXX` || (echo "mktemp failed"; exit 1)

echo "Using output dir $dir"

../ns_exec -cp ./ckpt  $dir
if [ ! -f $dir/cr-test.out ]; then
	echo "FAIL - ckpt did not create $dir/cr-test.out"
	exit 1
fi
v=`grep ret $dir/cr-test.out | awk -F=  '{ print $2 '}`
if [ "x$v" == "x" ]; then
	echo "FAIL - ckpt did not put its return value in $dir/cr-test.out"
	echo "Chances are sys_checkpoint() failed"
	echo "Please see $dir/cr-test.out for more details"
	exit 2
fi
if [ ! $v -gt 0 ]; then
	echo "FAIL - ckpt return value was $v, should be > 0"
	exit 3
fi

(cd $dir; $RESTART --pidns -l rlog -i out)
ret=$?
if [ $ret -ne 0 ]; then
	echo "return code was $ret"
fi
v=`grep ret $dir/cr-test.out | awk -F=  '{ print $2 }'`
if [ "x$v" == "x" ]; then
	echo "FAIL - restart return value was not in $dir/cr-test.out"
	exit 4
fi
if [ $v -ne 0 ]; then
	echo "FAIL - recorded return value was $v, should be == 0"
	exit 5
fi
echo PASS
exit 0
