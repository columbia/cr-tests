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
MKTREE=`which mktree`

freeze()
{
	echo FROZEN > ${freezermountpoint}/1/freezer.state
}

thaw()
{
	echo THAWED > ${freezermountpoint}/1/freezer.state
}

./userns_ckptme &
sleep 1
job=`jobs -p`
freeze
sleep 1
$CKPT $job > o.userns
ps -ef | grep userns_ckptme | grep -v grep > psout
correct_nlines=`cat psout | wc -l`
thaw
killall userns_ckptme

$MKTREE < o.userns &
sleep 1
ps -ef | grep userns_ckptme | grep -v grep > psout

lines=`cat psout | wc -l`
rootlines=`grep root psout | wc -l`
killall userns_ckptme
if [ $lines -ne $correct_nlines ]; then
	echo FAIL - there were only $lines restarted tasks, not $correct_nlines
	echo However that could well be a restart block failure, so let\'s ignore
fi
if [ $rootlines -ne $((lines-1)) ]; then
	echo FAIL - there were $rootlines restarted tasks owned by root out of $lines total
	exit 2
fi

echo PASS
exit 0
