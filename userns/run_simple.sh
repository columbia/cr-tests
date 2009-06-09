#!/bin/bash
# Copyright 2009 IBM Corp.
# Author: Serge Hallyn

source ../common.sh

rm -rf sandbox
mkdir sandbox
./simple_deep &
settimer 5
while [ ! -f sandbox/started ]; do : ; done
canceltimer

job=`jobs -p | head -1`
freeze
echo "Checkpointing job $job"
$CKPT $job > o.simple
thaw
killall simple_deep

echo "Restarting jobs"
$MKTREE < o.simple &

touch sandbox/go
touch sandbox/die

echo "Waiting for jobs to restart and complete"
settimer 5
while [ ! -f sandbox/status ]; do : ; done
canceltimer

echo PASS
exit 0
