#!/bin/bash

TEST_LOG=`mktemp -p . runtests-XXXXXXX`.log

echo Logfile: $TEST_LOG

> $TEST_LOG

TESTS="eclone-1 eclone-2 eclone-3 eclone-4 eclone-5"
#TESTS="eclone-5"

for t in $TESTS
do
	echo "===== Test: $t" >> $TEST_LOG
	./$t >> $TEST_LOG 2>&1
	if [ $? -eq 0 ]; then
		echo "Test '$t' PASSED" >> $TEST_LOG
	else
		echo "Test '$t' FAILED" >> $TEST_LOG
	fi
done

cat $TEST_LOG
