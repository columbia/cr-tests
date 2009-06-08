#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

# We are playing with perms on /root, so make sure to clean up
# on exit
rootmode=`stat -c %a /root`
trap '\
set +eu ; set -x ; \
echo THAWED > "${freezermountpoint}/1/freezer.state" ; \
chmod $rootmode /root' EXIT

chmod 750 /root

rm -rf sandbox
mkdir sandbox
chown 501:501 sandbox
echo "Running usertask"
killall usertask
./usertask &
settimer 5
while [ ! -f sandbox/started ]; do : ; done
canceltimer
pid=`pidof usertask`

freeze
$CKPT $pid > ckpt.out
thaw
kill -9 $pid
$RSTR < ckpt.out &
touch sandbox/go

settimer 5
while [ ! -f sandbox/readytodie ]; do : ; done
canceltimer

touch sandbox/die

if [ -f sandbox/error ]; then
	echo "FAIL: unprivileged user read /root"
	exit 1
fi

uid=`head -1 sandbox/outfile|awk '{ print $1 '}`
gid=`head -1 sandbox/outfile|awk '{ print $2 '}`
numlines=`wc -l sandbox/outfile | awk '{ print $1 '}`
numgids=$((numlines-1))
ok=1
if [ $uid -ne 501 ]; then
	echo "wrong uid: $uid instead of 501"
	ok=0
fi
if [ $gid -ne 501 ]; then
	echo "wrong gid: $gid instead of 501"
	ok=0
fi
if [ $numgids -ne 0 ]; then
	echo "aux groups not cleared: there were $numgids extra groups"
	ok=0
fi
if [ $ok -ne 1 ]; then
	echo "FAIL: credentials not properly reset"
	exit 1
fi

echo "PASS: credentials reset and unprivileged user could not read /root"
exit 0
