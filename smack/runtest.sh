#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

smackload() {
	mount | grep smack
	if [ $? -ne 0 ]; then
		echo "BROK: please mount smackfs"
		exit 1
	fi
	s=`which smackload`
	if [ $? -ne 0 ]; then
		echo "BROK: please install smackload"
		exit 1
	fi
	cat smackpolicy | $s
	if [ $? -ne 0 ]; then
		echo "BROK: couldn't load policy"
		exit 1
	fi
	echo "policy loaded"
}

source ../common.sh
verify_freezer
verify_paths

smackload

rm -f ./cr-test.out out

echo "Creating a checkpoint image using task context vs1 for use in all tests"
echo vs1 > /proc/self/attr/current
./ckpt > out

echo "Test 1: existing contexts are maintained by default on retart"
echo vs2 > /proc/self/attr/current
${MKTREE} < out
context=`cat context`
if [ -z "$context" -o "$context" != "vs2" ]; then
	echo "FAIL: did not maintain context vs2 on restart"
	exit 1
fi
thaw
echo "PASS"

echo "Test 2: can we restore contexts on restart"
${MKTREE} -k < out
context=`cat context`
if [ -z "$context" -o "$context" != "vs1" ]; then
	echo "FAIL: did not restore context vs1 on restart"
	echo "(was $context)"
	exit 1
fi
thaw
echo "PASS"

capsh=`which capsh`
if [ $? -ne 0 ]; then
	echo "capsh not installed, not running the last test"
	exit 0
fi
echo "Testing whether privilege is required to set task context on restart"
$capsh --drop=cap_mac_admin -- -c ${MKTREE} -k < out
if [ $? -eq 0 ]; then
	echo "FAIL: we were allowed to restore context without cap_mac_admin"
	exit 1
fi

echo "Testing whether I can restart across smack policy versions"
v=`cat /smack/version`
v=$((v+1))
echo $v > /smack/version
${MKTREE} < out
ret1=$?
if [ $ret1 -ne 0 ]; then
	echo "FAIL failed to restart without KEEPLSM with new smack version"
	exit 1
fi
${MKTREE} -k < out
ret2=$?
if [ $ret2 -ne 1 ]; then
	echo "FAIL able to restart with KEEPLSM with new smack version"
	exit 1
fi

echo "All smack tests passed"
exit 0
