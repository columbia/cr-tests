#!/bin/bash

killall pidns
rm -f checkpoint checkpoint-ready mypid.1 mypid.2 checkpoint-done

nsexec -cmgp -P outpid ./pidns &

while [ ! -f checkpoint-ready ]; do : ; done
outpid=`cat outpid`
echo started job $outpid
echo FROZEN > /cgroup/$outpid/freezer.state
checkpoint $outpid > checkpoint
echo press any key to continue
read x
kill -9 $outpid
echo THAWED > /cgroup/$outpid/freezer.state
touch checkpoint-done
chmod 666 checkpoint-done
restart --pids -i checkpoint
