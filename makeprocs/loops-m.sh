#!/bin/bash
# for each memory size t1..t4, do 30 c/r runs with clean memory

t1=1000000
t2=10000000
t3=100000000
t4=1000000000
sizes=( $t1 $t2 $t3 $t4 )

for s in ${sizes[@]}
do
	rm -f logf.$s.*
done

for s in ${sizes[@]}
do
	for i in `seq 1 30`; do
		echo "starting round $i (memsize $s)"
		sh runtest.sh -n 10 -m $s > logf.$s.$i 2>&1
	done
done
