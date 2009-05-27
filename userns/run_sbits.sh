#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer freezer /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`
mkdir $freezermountpoint/1 > /dev/null 2>&1

CKPT=`which ckpt`
RSTR=`which rstr`

freeze()
{
	echo FROZEN > ${freezermountpoint}/1/freezer.state
}

thaw()
{
	echo THAWED > ${freezermountpoint}/1/freezer.state
}

do_yer_thang()
{
	sleep 1
	pid=`pidof sbits`
	if [ "x$pid" == "x" ]; then
		echo "FAIL: sbits not running" >> outfile
		exit 1
	fi;
	freeze
	sleep 0.3
	$CKPT $pid > out.$pid
	thaw
	killall sbits
	$RSTR < out.$pid &
	sleep 0.3
	sleep 3
	killall sbits
}

expected_results=( "x:0:0:0" \
	"x:0:16:16" \
	"x:0:5:5" \
	"x:0:15:15" )

interpret_lineno()
{
	lno=$1
	if [ $lno -eq 0 ]; then
		echo "That line shouldn't have mattered!\n"
	elif [ $lno -eq 1 ]; then
		echo "That is the value before setting securebits"
	elif [ $lno -eq 2 ]; then
		echo "That is the value before checkpoint"
	elif [ $lno -eq 3 ]; then
		echo "That is the value after checkpoint"
	else
		echo "That reult number is not valid!"
	fi
}

parse_output()
{
	lno=0
	testno=0
	cat outfile | while read line; do
		if [ $lno -gt 3 ]; then
			lno=0
			testno=$((testno+1))
			if [ $testno -gt 3 ]; then
				echo "BROK: too many lines in outfile"
				exit 1
			fi
		fi
		if [ $lno -gt 0 ]; then
			resline=${expected_results[$testno]}
			no=$((lno+1))
			rightv=`echo $resline | awk -v t=$no -F: '{ print $t '}`
			realv=`echo $line | awk -F: '{ print $2 '}`
			if [ $rightv -ne $realv ]; then
				echo "FAIL: wrong value test $testno line $no"
				interpret_lineno $lno
				exit 1
			fi
		fi
		lno=$((lno+1));
	done	
}

rm -f outfile
killall sbits
# run without changing securebits
echo "Test 1: no securebits" >> outfile
./sbits &
do_yer_thang

# run with keepcaps set
echo "Test 2: with keepcaps" >> outfile
./sbits -k &
do_yer_thang

# run with noroot
echo "Test 3: with noroot" >> outfile
./sbits -r &
do_yer_thang

# run with noroot locked
echo "Test 4: with noroot locked" >> outfile
./sbits -r -l &
do_yer_thang

parse_output

echo PASS
exit 0
