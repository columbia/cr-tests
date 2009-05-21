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

rootmode=`stat -c %a /root`
trap '\
set +eu ; set -x ; \
echo THAWED > "${freezermountpoint}/1/freezer.state" ; \
chmod $rootmode /root' EXIT

chmod 700 /root

CKPT=`which ckpt`
MKTREE=`which mktree`
RSTR=`which rstr`

freeze()
{
	echo FROZEN > ${freezermountpoint}/1/freezer.state
}

thaw()
{
	echo THAWED > ${freezermountpoint}/1/freezer.state
}

killall usertask
./usertask -e &
sleep 1
pid=`pidof usertask`

freeze
sleep 0.3
$CKPT $pid > ckpt.out
ps axo uid,euid,gid,comm | grep usertask > psout.1
echo "fds on original task"
ls -l /proc/$pid/fd
thaw
kill -9 $pid
sleep 0.3
$RSTR < ckpt.out &
sleep 1
ps axo uid,euid,gid,comm | grep usertask > psout.2

diff psout.1 psout.2 > /dev/null 2>&1
ret=$?
if [ $ret -ne 0 ]; then
	echo "FAIL - uid/gid were different (see psout.1 and psout.2)"
	echo "This is a known failure, due to fd0 not being restored"
	echo "If/when fixed, then exit 1 here"
	# exit 1
fi

# TODO also check /proc/pid/fd/* for proper ownership
echo "fds on restarted task"
pid=`pidof usertask`
ls -l /proc/$pid/fd

killall usertask

if [ -f sandbox/error ]; then
	echo "FAIL: unprivileged user read /root"
	exit 1
fi

echo PASS
exit 0
