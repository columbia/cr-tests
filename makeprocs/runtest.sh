#!/bin/bash
echo "Running with $*"

rm -f out ready done checkpoint-done

nsexec -cgmpnuti -P pid ./mp $* &

while [ ! -f "ready" ]; do : ; done
pid=`cat pid`

echo FROZEN > /cgroup/$pid/freezer.state
sleep 2s
/usr/bin/time -p checkpoint $pid -o out
echo THAWED > /cgroup/$pid/freezer.state

touch "./checkpoint-done"
killall -9 mp

ret=0
while [ $ret -eq 0 ]; do
	pidof mp > /dev/null
	ret=$?
done

/usr/bin/time -p restart -i out -p -W

killall -9 mp
ret=0
while [ $ret -eq 0 ]; do
	pidof mp > /dev/null
	ret=$?
done
