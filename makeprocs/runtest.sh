#!/bin/bash
echo "Running with $*"

rm -f out out2 ready done checkpoint-done

#read -p "Press enter to start... " x
nsexec -cgmpnuti -P pid ./mp $* &
sleep 1
pid=`cat pid`

while [ ! -f "ready" ]; do : ; done

echo FROZEN > /cgroup/$pid/freezer.state
/usr/bin/time -p checkpoint $pid -o out
echo THAWED > /cgroup/$pid/freezer.state

touch "./checkpoint-done"

/usr/bin/time -p restart -i out -p
