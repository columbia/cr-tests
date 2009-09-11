#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Sukadev Bhattiprolu <sukadev@us.ibm.com>

freezermountpoint=/cgroup
CHECKPOINT="../"
NS_EXEC="../ns_exec"
CR=`which checkpoint`
RSTR=`which restart`

SLOW_DOWN="slow-down-fileio"
CKPT_FILE="ckpt-fileio1";
CKPT_READY="checkpoint.ready"
COPY_DONE="copy.done"
SRC_FILE="input-data.1";
DEST_FILE="output-data.1";
DEST_FILE_SNAP="output-data.1.snap";
TEST_LOG="log.fileio1"
TEST_LOG_SNAP="log.fileio1.snap"

LOG_FILE="f1-loop.log"
TEST_CMD="./fileio1"

freeze()
{
	mkdir ${freezermountpoint}/$1
	echo $1 > ${freezermountpoint}/$1/tasks
	sleep 0.3
	/bin/echo FROZEN > ${freezermountpoint}/$1/freezer.state
	sleep 0.3
}

unfreeze()
{
	/bin/echo THAWED > ${freezermountpoint}/$1/freezer.state
	echo $1 > ${freezermountpoint}/tasks
	sleep 0.3
	rmdir ${freezermountpoint}/$1
}

# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	echo "please mount freezer cgroups"
	echo "  mkdir /cgroup"
	echo "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
freezermountpoint=`echo $line | awk '{ print $2 '}`

# Make sure no stray TEST_CMD from another run is still going
killall $TEST_CMD

#echo > $LOG_FILE

#Create the SRC_FILE
$TEST_CMD -C $SRC_FILE

cnt=1
sleep_time=3;

NUMTESTS=5
for testnum in `seq 1 $NUMTESTS`; do
	echo "----- Iteration $cnt"

	# Copy file slowly, so we can checkpoint
	touch $SLOW_DOWN

	# Remove CKPT_READY file, start the application and let app tell
	# us when it is ready
	rm -f $CKPT_READY;
	$NS_EXEC -m $TEST_CMD -c $SRC_FILE $DEST_FILE &
	while [ ! -f $CKPT_READY ]; do
		sleep 1;
	done;

	# Let it run for a while before checkpointing
	echo "Created $TEST_CMD process, sleep $sleep_time"
	sleep $sleep_time

	pid=`pidof $TEST_CMD`
	if [  "x$pid" == "x" ]; then
		echo "$TEST_CMD is not running!  pid is $pid.  fail"
		ps -ef |grep $TEST_CMD
		exit 1
	fi

	freeze $pid

	# Checkpoint
	echo $CR $pid > $CKPT_FILE
	$CR $pid > $CKPT_FILE
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "===== Checkpoint of $pid failed"
		ps aux |grep $TEST_CMD
		exit 1;
	fi

	# Snapshot the outfile and log file
	cp $DEST_FILE $DEST_FILE_SNAP
	cp $TEST_LOG $TEST_LOG_SNAP 

	ls -l $SRC_FILE $DEST_FILE

	unfreeze $pid

	kill -9 $pid

	# Restore the snapshot after the main process has been killed
	cp ${DEST_FILE}.snap $DEST_FILE

	cp $TEST_LOG_SNAP $TEST_LOG

	# Remove COPY_DONE file. We will wait below for application to
	# finish copying and let us know.
	rm -f $COPY_DONE;

	# Restart.
	$NS_EXEC -m rstrsh $CKPT_FILE &
	ret=$?

	if [ $ret -ne 0 ]; then
		echo "===== Restart of $pid failed"
		ps aux |grep $TEST_CMD
		exit 1;
	fi

	# Find pid of restarted test cmd...

	sleep 1;
	pid=`pidof $TEST_CMD`
	if [ "x$pid" == "x" ]; then
		echo "Can't find pid of $TEST_CMD"
		exit 1
	fi

	nspid=`pidof $NS_EXEC`
	if [ "x$nspid" == "x" ]; then
		echo "Can't find pid of $NS_EXEC"
		exit 1
	fi

	# ...then zip through rest of copy.
	rm $SLOW_DOWN

	wait $nspid;
	ret=$?

	echo "$nspid exited, status $ret"

	#ls -l $SRC_FILE $DEST_FILE $CKPT_FILE

	/usr/bin/cmp $SRC_FILE $DEST_FILE
	if [ $? -ne 0 ]; then
		echo "file copy ($SRC_FILE -> $DEST_FILE) failed after restart"
		exit 1;
	fi

	cnt=$((cnt+1))

	# Change delay so next checkpoint happens at a different point
	sleep_time=$((sleep_time+1))
	if [ $sleep_time -gt 10 ]; then
		sleep_time=2;
	fi
done

echo PASS

exit 0
