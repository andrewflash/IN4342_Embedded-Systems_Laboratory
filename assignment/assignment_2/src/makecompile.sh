#!/bin/bash
pathdir=/home/root/esLAB/angga/code 
cd gpp
make clean
make
cd ../dsp
make clean
make
cd ..