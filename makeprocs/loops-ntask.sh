#!/bin/bash
# for each number of tasks t1..t4, do 30 c/r runs

t1=10
t2=100
t3=1000
t4=10000
sizes=( $t1 $t2 $t3 $t4 )

for s in ${sizes[@]}
do
	rm -f logf.$s.*
done

ulimit -u 0

for s in ${sizes[@]}
do
	for i in `seq 1 30`; do
		echo "starting round $i (ntasks $s)"
		sh runtest.sh -n $s > logf.$s.$i 2>&1
		mv out ckpt.ntask.$s.$i
	done
done
