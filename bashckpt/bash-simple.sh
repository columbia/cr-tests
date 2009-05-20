#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Nathan Lynch

set -eu

tmpdir="/tmp/bash-$1"

step1go="$tmpdir/step1-go"
step1ok="$tmpdir/step1-ok"
step2go="$tmpdir/step2-go"
step2ok="$tmpdir/step2-ok"

pidfile="$tmpdir/pid-there"

logfile="$tmpdir/bash-simple-$$.log"

# close stdin
exec <&-

# redirect stdio/stderr to file
exec 1>"$logfile"
exec 2>&1

ls -l /proc/$$/fd

cat /proc/$$/maps

echo $$ > $pidfile

while [ ! -f $step1go ] ; do : ; done

wait

echo "Step 1 OK."
echo > $step1ok

# wait for checkpoint -- just spin, don't fork a task for sleep
while [ ! -f $step2go ] ; do : ; done

# restarted

echo "Step 2 OK."

#cat /proc/$$/maps

echo > $step2ok
