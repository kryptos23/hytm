#!/bin/bash

hostname="zyra"
compile="./compile_x64.sh 'pinning=ZYRA_CLUSTER'"
runcmd="hytm2_3path -t 1000 -mr debra -ma new -mp none -k 10 -i 50 -d 50 -rq 0 -rqsize 256 -nwork 1 -nrq 0"

rm temp.txt
eval "${compile}" || (echo "ERROR: failed to eval: ${compile}" && exit 1)

echo "## Testing... concurrently run 'watch ./testing_validation_watch.sh' to watch for errors"
for ((i=0;i<1000;++i)) ; do
    LD_PRELOAD=../../lib/libjemalloc.so numactl --interleave=all ../bin/${hostname}.dsbench_${runcmd} | grep Validation | tee -a temp.txt
done
