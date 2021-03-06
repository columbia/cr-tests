#!/bin/bash

source ../common.sh

dir=`mktemp -p . -d -t cr_pthread2_XXXXXXX` || (echo "mktemp failed"; exit 1)
mkdir $dir
echo "Using output dir $dir"
cd $dir

# NOTE: As of ckpt-v15-dev, the --container option to 'ckpt' causes this
#	test to fail with "container not isolated" message due to the
#	log-file being shared between the application threads.
#
CHECKPOINT="`which checkpoint` --container"
RESTART=`which restart`
ECHO="/bin/echo -e"

TEST_CMD="../pthread2"
TEST_ARGS="-n 4"			# -n: number of threads
SCRIPT_LOG="log-run-pthread2"
TEST_PID_FILE="pid.pthread2";

SNAPSHOT_DIR="snap1.d"

TEST_DONE="test-done"
CHECKPOINT_FILE="checkpoint-pthread2";
CHECKPOINT_READY="checkpoint-ready"
CHECKPOINT_DONE="checkpoint-done"

LOGS_DIR="logs.d"

NSEXEC_ARGS="-cgpuimP $TEST_PID_FILE"

checkpoint()
{
	local pid=$1

	$ECHO "Checkpoint: $CHECKPOINT $pid \> $CHECKPOINT_FILE"
	$CHECKPOINT $pid > $CHECKPOINT_FILE
	ret=$?
	if [ $ret -ne 0 ]; then
		$ECHO "***** FAIL: Checkpoint of $pid failed"
		ps -efL |grep $TEST_CMD >> $SCRIPT_LOG
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

function create_container()
{
	local pid;

	cmdline="$NSEXEC $NSEXEC_ARGS -- $TEST_CMD $TEST_ARGS"

	$ECHO "\t- Creating container:"
	$ECHO "\t- $cmdline"

	$cmdline &

	wait_for_checkpoint_ready;

	# Find global pid of container-init
	pid=`cat $TEST_PID_FILE`;
	if [  "x$pid" == "x" ]; then
		$ECHO "***** FAIL: Invalid container-init pid $pid"
		ps -efL |grep $TEST_CMD >> $SCRIPT_LOG
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
		ps -efL |grep $TEST_CMD >> $SCRIPT_LOG
		exit 1;
	fi
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

# Make sure no stray pthread1 from another run is still going
killall $TEST_CMD > $SCRIPT_LOG 2>&1

if [ ! -d $LOGS_DIR ]; then
	mkdir $LOGS_DIR
fi

if [ ! -d $DATA_DIR ]; then
	mkdir $DATA_DIR
fi

if [ ! -d $SNAPSHOT_DIR ]; then
	mkdir $SNAPSHOT_DIR
fi

if [ ! -f $INPUT_DATA ]; then
	$FILEIO -C $INPUT_DATA
fi

> $SCRIPT_LOG;
cnt=1
while [ $cnt -lt 15 ]; do
	$ECHO "===== Iteration $cnt"

	# Remove any 'state' files, start the app and let it tell us
	# when it is ready
	rm -f $CHECKPOINT_READY $TEST_DONE $TEST_PID_FILE

	create_container
	wait_for_checkpoint_ready

	pid=`cat $TEST_PID_FILE`

	$ECHO "\t- Done creating container, cinit-pid $pid"

	ps -efL |grep $TEST_CMD >> $SCRIPT_LOG

	# override default freezerdir
	if [ -d $freezerdir ]; then
		rmdir $freezerdir
	fi
	freezerdir=$freezermountpoint/$pid
	freeze_pid $pid

	num_pids1=`ps -efL |grep $TEST_CMD | wc -l`

	create_fs_snapshot

	checkpoint $pid

	touch $CHECKPOINT_DONE

	killall -9 `basename $TEST_CMD`

	thaw

	sleep 3

	restore_fs_snapshot

	restart_container

	sleep 3;

	num_pids2=`ps -efL |grep $TEST_CMD | wc -l`
	ps -efL |grep $TEST_CMD >> $SCRIPT_LOG
	$ECHO "\t- num_pids1 $num_pids1, num_pids2 $num_pids2";

	# nsexec pid is parent-pid of restarted-container-init
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

	cnt=$((cnt+1))
done
