#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Nathan Lynch

set -eu

source ../common.sh
tmpdir=`mktemp -p . -d -t cr_bash_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $tmpdir"

cgroup=$freezerdir

step1go="$tmpdir/step1-go"
step1ok="$tmpdir/step1-ok"
step2go="$tmpdir/step2-go"
step2ok="$tmpdir/step2-ok"

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
./bash-simple.sh $$ $tmpdir &

# note: $! is the pid of nsexec, not the command, so we need to be a
# little more clever to derive the pid to pass to ip link set netns

echo -n "Waiting for background script to start... "
while [ ! -f $pidfile ] ; do sleep 1 ; done
read there_pid < $pidfile
echo "$there_pid."

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

echo "PASS."
echo
echo
exit 0
