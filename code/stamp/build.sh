#!/bin/bash

PROGS="bayes genome intruder kmeans labyrinth ssca2 vacation yada"
#TARGETS="seq seqtm tl2 hytm1 hytm2"

PROGS="kmeans"
TARGETS="hytm2"

mkdir bin

for t in ${TARGETS}
do
    # compile the TM library
    if [ $t != "seq" ]; then
        cd $t
        make clean
        make
        if [ $? -ne 0 ]; then echo "ERROR: failed to compile TM library $t"; exit -1; fi
        cd ..
        echo "Compiled TM library: $t"

        mfile="Makefile.stm"
    else
        mfile="Makefile.seq"
    fi

    # compile the benchmarks that will use the library
    for p in ${PROGS}
    do
        cd $p
        make -f $mfile clean TARGET=$t
        make -f $mfile TARGET=$t
        if [ $? -ne 0 ]; then "ERROR: failed to compile benchmark $p for TM library $t"; exit -1; fi
        mv ${p}_${t} ../bin/
        cd ..
        echo "Compiled benchmark $p for TM library $t"
    done     	
done
echo "Compiled all benchmarks successfully (I think...)"