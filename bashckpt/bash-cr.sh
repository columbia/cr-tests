#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Nathan Lynch

set -eu
echo 1

CHECKPOINT=`which checkpoint`
RESTART=`which restart`

tmpdir="/tmp/bash-$$"
rm -rf $tmpdir
mkdir $tmpdir

echo 2
# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroup"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
cgroup_mnt=`echo $line | awk '{ print $2 '}`
cgroup="$cgroup_mnt/bash-test-$$"

echo 3
step1go="$tmpdir/step1-go"
step1ok="$tmpdir/step1-ok"
step2go="$tmpdir/step2-go"
step2ok="$tmpdir/step2-ok"
echo 4

pidfile="$tmpdir/pid-there"

there_pid=

trap '\
set +eu ; set -x ; \
echo THAWED > "$cgroup/freezer.state" ; \
kill -9 $! &>/dev/null ; \
wait $! ; \
rmdir "$cgroup" ; \
echo results in $tmpdir ' EXIT

echo "Running ./bash-simple.sh in background."
./bash-simple.sh $$ &

# note: $! is the pid of ns_exec, not the command, so we need to be a
# little more clever to derive the pid to pass to ip link set netns

echo -n "Waiting for background script to start... "
while [ ! -f $pidfile ] ; do sleep 1 ; done
read there_pid < $pidfile
echo "$there_pid."

echo "Creating cgroup $cgroup."
mkdir "$cgroup"

    # need to assign cpus and mems to our new cgroup
for knob in cpuset.cpus cpuset.mems ; do
    test -f "$cgroup_mnt/$knob" && \
	cat "$cgroup_mnt/$knob" > "$cgroup/$knob"
done

echo $there_pid > "$cgroup/tasks"

touch $step1go

while [ ! -f $step1ok ] ; do sleep 1 ; done

# child now spinning on step2go, checkpoint it

echo "Freezing cgroup $cgroup."
echo FROZEN > "$cgroup/freezer.state"
read freezer_state < "$cgroup/freezer.state"

# need to keep trying, is this a bug in my script or the kernel?
while [ "$freezer_state" != "FROZEN" ] ; do
    echo FROZEN > "$cgroup/freezer.state"
    read freezer_state < "$cgroup/freezer.state"
    sleep 1
done

ckptfile="$tmpdir/ckpt-$there_pid"

echo "Checkpointing $there_pid to $ckptfile."
$CHECKPOINT $there_pid > $ckptfile

echo "Killing $there_pid."
kill -9 $there_pid
echo THAWED > "$cgroup/freezer.state"
wait

echo "Restarting from $ckptfile."
$RESTART --pids < $ckptfile &

touch $step2go

echo "Waiting for restarted script to run."
cnt=1
while [ ! -f $step2ok ] ; do
	cnt=$((cnt+1))
	if [ $cnt -gt 10 ]; then
		echo FAIL: did not restart
		exit 1
	fi
	sleep 1
done

echo "Pass."
exit 0
