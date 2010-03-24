#!/bin/bash
# start as 'runtest.sh p' to have it pause after checkpoint
# but before thawing the original program (to examine task state)
# just running as 'runtest.sh' will run fully automatically

dir=`mktemp -p . -d -t cr_pidns_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
killall pidns

cd $dir

pause=0
if [ $# -gt 0 ]; then
	pause=1
fi

nsexec -cmg -P outpid ../pidns &

while [ ! -f checkpoint-ready ]; do : ; done
outpid=`cat outpid`
echo started job $outpid
echo FROZEN > /cgroup/$outpid/freezer.state
checkpoint $outpid > checkpoint
if [ $pause -eq 1 ]; then
	echo press any key to continue
	read x
fi
kill -9 $outpid
echo THAWED > /cgroup/$outpid/freezer.state
touch checkpoint-done
rm -f  mypid.2
chmod 666 checkpoint-done
restart --pids -i checkpoint
wait

if [ ! -f mypid.2 ]; then
	echo "FAIL: mypid.2 was not created"
	exit 1
fi

p1=`cat mypid.1`
p2=`cat mypid.2`
if [ "x$p1" != "x$p2" ]; then
	echo "FAIL: pid $p1 was restarted as $p2"
	exit 2
fi

echo "PASS: successfully restarted pid $p2 in nested pidns."
exit 0
