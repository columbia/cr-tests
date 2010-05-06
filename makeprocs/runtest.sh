#!/bin/bash
echo "Running with $*"

rm -f out out2 ready done checkpoint-done

read -p "Press enter to start... " x
nsexec -cgmpnuti -P pid ./mp $* &
sleep 1
pid=`cat pid`

while [ ! -f "ready" ]; do : ; done

echo FROZEN > /cgroup/$pid/freezer.state
#/usr/bin/time checkpoint $pid -o out
#/usr/bin/time checkpoint $pid -o out2
time checkpoint $pid -o out
time checkpoint $pid -o out2
echo THAWED > /cgroup/$pid/freezer.state

touch "./checkpoint-done"

#/usr/bin/time restart -i out -p
time restart -i out -p
