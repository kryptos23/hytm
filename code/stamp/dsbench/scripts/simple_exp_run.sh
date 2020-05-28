#!/bin/bash

echo MAXKEY INSERT_FRAC ALGO throughput > temp.txt

for k in 100000 10000 ; do
for uhalf in 0 1 5 20 50 ; do
for alg in hytm1 hytm2 hytm2_3path hytm3 tl2 ; do

    cmd="LD_PRELOAD=../../lib/libjemalloc.so numactl --interleave=all"\
" ../bin/$(hostname).dsbench_${alg} -t 1000 -mr debra -ma new -mp none -p"\
" -k $k -i $uhalf -d $uhalf -rq 0 -rqsize 256 -nwork 36 -nrq 0 >> run.txt"

    echo "$cmd" > run.txt
    eval "$cmd"

    fields run.txt $(head -1 temp.txt) >> temp.txt

done ; done ; done
