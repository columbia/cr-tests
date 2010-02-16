#!/bin/bash

source ../common.sh

dir=`mktemp -p . -d -t cr_ptree_XXXXXXX` || (echo "mktemp failed"; exit 1)
echo "Using output dir $dir"
cd $dir

BASE_DIR="../.."

FILEIO="../../fileio/fileio1"

ECHO="/bin/echo -e"

TEST_CMD="../ptree1"

# -n: children per process, -d: depth of process tree
TEST_ARGS="-n 2 -d 1 -w sleep"

SCRIPT_LOG="log-run-ptree1"
TEST_PID_FILE="pid.ptree1";
SNAPSHOT_DIR="snap1.d"
LOGS_DIR="logs.d"
DATA_DIR="data.d"

TEST_DONE="test-done"
CHECKPOINT_FILE="checkpoint-ptree1";
CHECKPOINT_READY="checkpoint-ready"
CHECKPOINT_DONE="checkpoint-done"
INPUT_DATA="input.data";

NSEXEC_ARGS="-cgpuimP $TEST_PID_FILE"


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

	cmdline="$NSEXEC $NSEXEC_ARGS -- $TEST_CMD $TEST_ARGS"

	$ECHO "\t- Creating container:"
	$ECHO "\t- $cmdline"

	$cmdline &

	j=0;
	# Wait for test to finish setup
	while [ ! -f $CHECKPOINT_READY ]; do
		$ECHO "\t- Waiting for $CHECKPOINT_READY"
		sleep 1;
		j=`expr $j + 1`;
		if [ $j -eq 30 ]; then
			$ECHO "\t ***** FAIL No $CHECKPOINT_READY"
			exit 1;
		fi
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

	# Snapshot the log and data files files
	cp -r ${LOGS_DIR} ${DATA_DIR} $SNAPSHOT_DIR
}

function restore_fs_snapshot()
{
	# Restore the snapshot after the main process has been killed
	/bin/cp -r ${SNAPSHOT_DIR}/* .
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

# Make sure no stray ptree1 from another run is still going
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
	pid=`cat $TEST_PID_FILE`

	$ECHO "\t- Done creating container, cinit-pid $pid"

	wait_for_checkpoint_ready
	ps aux |grep $TEST_CMD >> $SCRIPT_LOG

	# override default freezerdir
	if [ -d $freezerdir ]; then
		rmdir $freezerdir
	fi
	freezerdir=$freezermountpoint/$pid
	freeze_pid $pid

	num_pids1=`ps aux |grep $TEST_CMD | wc -l`

	create_fs_snapshot

	sleep 1;

	checkpoint $pid

	touch $CHECKPOINT_DONE

	killall -9 `basename $TEST_CMD`

	thaw

	sleep 3

	restore_fs_snapshot

	restart_container

	sleep 3;

	num_pids2=`ps aux |grep $TEST_CMD | wc -l`
	ps aux |grep $TEST_CMD >> $SCRIPT_LOG
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
