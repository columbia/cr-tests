#!/bin/bash

freezermountpoint=/cgroup
CHECKPOINT=".."

# NOTE: As of ckpt-v15-dev, the --container option to 'ckpt' causes this
#	test to fail with "container not isolated" message due to the
#	log-file being shared between the application threads.
#
CHECKPOINT="`which checkpoint` --container"
RESTART=`which restart`
ECHO="/bin/echo -e"

TEST_CMD="./pthread1"
TEST_ARGS="-n 4"			# -n: number of threads
SCRIPT_LOG="log-run-pthread1"
TEST_PID_FILE="pid.pthread1";

SNAPSHOT_DIR="snap1.d"

TEST_DONE="test-done"
CHECKPOINT_FILE="checkpoint-pthread1";
CHECKPOINT_READY="checkpoint-ready"
CHECKPOINT_DONE="checkpoint-done"

LOGS_DIR="logs.d"

NS_EXEC="../ns_exec"
NS_EXEC_ARGS="-cgpuimP $TEST_PID_FILE"

freeze()
{
	$ECHO "\t - Freezing $1"
	fnam="${freezermountpoint}/$1/freezer.state"
	$ECHO FROZEN > $fnam
	while [ `cat $fnam` != "FROZEN" ]; do
		$ECHO FROZEN > $fnam
	done
}

unfreeze()
{
	$ECHO "\t - Unfreezing $1"
	$ECHO THAWED > ${freezermountpoint}/$1/freezer.state
}

cleancgroup()
{
	$ECHO "\t - Clean cgroup of $1"
	rmdir ${freezermountpoint}/$1
	if [ -d ${freezermountpoint}/$1 ]; then
		$ECHO ***** WARNING ${freezermountpoint}/$1 remains
	fi
}

checkpoint()
{
	local pid=$1

	$ECHO "Checkpoint: $CHECKPOINT $pid \> $CHECKPOINT_FILE"
	$CHECKPOINT $pid > $CHECKPOINT_FILE
	ret=$?
	if [ $ret -ne 0 ]; then
		$ECHO "***** FAIL: Checkpoint of $pid failed"
		ps aux |grep $TEST_CMD >> $SCRIPT_LOG
		exit 1;
	fi
}

function create_container()
{
	local pid;

	cmdline="$NS_EXEC $NS_EXEC_ARGS -- $TEST_CMD $TEST_ARGS"

	$ECHO "\t- Creating container:"
	$ECHO "\t- $cmdline"

	$cmdline &

	# Wait for test to finish setup
	j=0;
	while [ ! -f $CHECKPOINT_READY ]; do
		$ECHO "\t- Waiting for $CHECKPOINT_READY"

		j=`expr $j + 1`;
		if [ $j -eq 30 ]; then
			$ECHO "\t ***** FAIL No $CHECKPOINT_READY"
			exit 1;
		fi
		sleep 1;
	done;

	# Find global pid of container-init
	pid=`cat $TEST_PID_FILE`;
	if [  "x$pid" == "x" ]; then
		$ECHO "***** FAIL: Invalid container-init pid $pid"
		ps aux |grep $TEST_CMD >> $SCRIPT_LOG
		exit 1
	fi
	$ECHO "Created container with pid $pid" >> $SCRIPT_LOG
}

function restart_container
{
	local ret;

	cmdline="$RESTART --pids --pidns --wait"
	$ECHO "\t- $cmdline"

	sleep 1

	$cmdline < $CHECKPOINT_FILE >> $SCRIPT_LOG 2>&1 &
	ret=$?

	if [ $ret -ne 0 ]; then
		$ECHO "***** FAIL: Restart of $pid failed"
		ps aux |grep $TEST_CMD >> $SCRIPT_LOG
		exit 1;
	fi
}

function wait_for_checkpoint_ready()
{
	# Wait for test to finish setup
	while [ ! -f $CHECKPOINT_READY ]; do
		$ECHO "\t- Waiting for $CHECKPOINT_READY"
		sleep 1;
	done;
}

function create_fs_snapshot()
{
	# Prepare for snapshot
	if [ -d $SNAPSHOT_DIR ]; then
		rm -rf ${SNAPSHOT_DIR}.prev
		mv $SNAPSHOT_DIR ${SNAPSHOT_DIR}.prev
		mkdir $SNAPSHOT_DIR
	fi

	# Snapshot the log files
	cp ${LOGS_DIR}/* $SNAPSHOT_DIR
}

function restore_fs_snapshot()
{
	# Restore the snapshot after the main process has been killed
	/bin/cp ${SNAPSHOT_DIR}/* $LOGS_DIR
}


# Check freezer mount point
line=`grep freezer /proc/mounts`
if [ $? -ne 0 ]; then
	$ECHO "please mount freezer cgroup"
	$ECHO "  mkdir /cgroup"
	$ECHO "  mount -t cgroup -o freezer cgroup /cgroup"
	exit 1
fi
#freezermountpoint=`$ECHO $line | awk '{ print $2 '}`

# Make sure no stray pthread1 from another run is still going
killall $TEST_CMD > $SCRIPT_LOG 2>&1

> $SCRIPT_LOG;
cnt=1
while [ $cnt -lt 15 ]; do
	$ECHO "===== Iteration $cnt"

	# Remove any 'state' files, start the app and let it tell us
	# when it is ready
	rm -f $CHECKPOINT_READY $TEST_DONE $TEST_PID_FILE

	create_container
	pid=`cat $TEST_PID_FILE`

	$ECHO "\t- Done creating container, cinit-pid $pid"

	wait_for_checkpoint_ready
	ps aux |grep $TEST_CMD >> $SCRIPT_LOG

	freeze $pid

	num_pids1=`ps aux |grep $TEST_CMD | wc -l`

	create_fs_snapshot

	checkpoint $pid

	touch $CHECKPOINT_DONE

	killall -9 `basename $TEST_CMD`

	unfreeze $pid

	sleep 3

	cleancgroup $pid

	restore_fs_snapshot

	restart_container

	sleep 3;

	num_pids2=`ps aux |grep $TEST_CMD | wc -l`
	ps aux |grep $TEST_CMD >> $SCRIPT_LOG
	$ECHO "\t- num_pids1 $num_pids1, num_pids2 $num_pids2";

	# ns_exec pid is parent-pid of restarted-container-init
	nspid=`pidof restart`

	if [ "x$nspid" == "x" ]; then
		$ECHO "***** FAIL: Can't find pid of $RESTART"
		exit 1;
	fi
	
	# End test gracefully
	touch $TEST_DONE

	$ECHO "\t- Waiting for restarted container to exit (gloabl-pid $nspid)"
	wait $nspid;
	ret=$?

	$ECHO "\t- Container exited, status $ret"

	if [ -d /cgroups/$pid ]; then
		cleancgroup $pid
	fi

	cnt=$((cnt+1))
done
