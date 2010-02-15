#!/bin/bash

echo
echo "****** $0: Running test: fileio1"
echo
./run-fileio1.sh

echo
echo "****** $0: Running test: filelock1"
echo "****** (expect FAIL)"
echo
./run-fcntltests.sh filelock1

echo
echo "****** $0: Running test: filelease1"
echo "****** (expect FAIL)"
echo
./run-fcntltests.sh filelease1

echo
echo "****** $0: Running test: fsetown1"
echo "****** (expect FAIL)"
echo
./run-fcntltests.sh fsetown1
