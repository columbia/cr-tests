#!/bin/bash

set -e
# Generate a random tree and check its contents
./rand_tree.py -r ./tree -d 4 -f 10 && trap 'rm -rf ./tree' EXIT
./check_tree.py -r ./tree -c original

# Fill its contents with new dummy values
./fill_tree.py -r ./tree -c dummy
./check_tree.py -r ./tree -c dummy

# Fill its contents with "original" values
./fill_tree.py -r ./tree -c original
./hold_unlinked_tree.py -r ./tree -n && \
# Expect "new" contents
./check_tree.py -r ./tree -c new
