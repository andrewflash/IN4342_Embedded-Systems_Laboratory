#!/bin/bash

pic=klomp.pgm

for iteration in `seq 10`;
do
	/home/root/powercycle.sh
	./canny pics/$pic >> results.txt
done