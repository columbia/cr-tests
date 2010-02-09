#!/bin/bash

source ../common.sh

if [ $# -lt 1 ]; then
	echo "Usage: $0 <test-case>";
	exit 1;
fi

test_case=$1;
shift

if [ ! -x $test_case ]; then
	echo "$0: Test case \'$test_case\' does not exist / not executable ?"
	exit 1;
fi

dir=`mktemp -p . -d -t cr_${test_case}_XXXXXXX` || (echo "mktemp failed"; exit 1)

# NOTE: As of ckpt-v15-dev, the --container option to 'ckpt' causes this
#	test to fail with "container not isolated" message due to the
#	log-file being shared between the application threads.
#
CHECKPOINT="`which checkpoint` --container"
RESTART=`which restart`
ECHO="/bin/echo -e"

TEST_CMD="../$test_case"
TEST_ARGS=$*
TEST_LOG="logs.d/log.${test_case}"
SCRIPT_LOG="logs.d/log.run-${test_case}"
TEST_PID_FILE="pid.${test_case}";

SNAPSHOT_DIR="snap1.d"

TEST_DONE="test-done"
CHECKPOINT_FILE="checkpoint-${test_case}";
CHECKPOINT_READY="checkpoint-ready"
CHECKPOINT_DONE="checkpoint-done"

LOGS_DIR="logs.d"
DATA_DIR="data.d"

NS_EXEC="../../ns_exec"
NS_EXEC_ARGS="-cgpuimP $TEST_PID_FILE"

checkpoint()
{
	local pid=$1

	$ECHO "\t- Checkpoint: $CHECKPOINT $pid \> $CHECKPOINT_FILE"
	$CHECKPOINT $pid > $CHECKPOINT_FILE
	ret=$?
	if [ $ret -ne 0 ]; then
		$ECHO "***** FAIL: Checkpoint of $pid failed"
		ps -efL |grep $TEST_CMD >> $SCRIPT_LOG
		killall -9 `basename $TEST_CMD`
		thaw
		exit 1;
	fi
}

function check_for_failure()
{
	grep --binary-files=text FAIL $PWD/$TEST_LOG > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		$ECHO "\t***** Application FAILED after restart" >> $SCRIPT_LOG
		$ECHO "\t***** See $TEST_LOG for details" >> $SCRIPT_LOG

		$ECHO "\t***** Application FAILED after restart"
		$ECHO "\tSee $PWD/$TEST_LOG for details"
		exit 1;
	fi
}

function wait_for_checkpoint_ready()
{
	# Wait for test to finish setup
	while [ ! -f $CHECKPOINT_READY ]; do
		$ECHO "\t- Waiting for $CHECKPOINT_READY"
		check_for_failure;
		sleep 1;
	done;
}

function create_container()
{
	local pid;

	cmdline="$NS_EXEC $NS_EXEC_ARGS -- $TEST_CMD $TEST_ARGS"

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

cd $dir
echo "Current directory: `pwd`"

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

# Make sure no stray test-case process from another run is still going
killall $TEST_CMD > $SCRIPT_LOG 2>&1

> $SCRIPT_LOG;
cnt=1
while [ $cnt -lt 20 ]; do
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

	check_for_failure;

	$ECHO "\t- Container exited, status $ret"

	cnt=$((cnt+1))
done
