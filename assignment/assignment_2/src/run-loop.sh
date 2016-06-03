#!/bin/bash

mkdir results
for iteration in `seq $2`;
do
	/home/root/powercycle.sh
	./pool_notify pool_notify.out $1 > results/results$iteration.txt
done
