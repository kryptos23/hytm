#!/bin/bash

PROGS="bayes genome intruder kmeans labyrinth ssca2 vacation yada"
#TARGETS="seq seqtm tl2 hytm1 hytm2"

#PROGS="kmeans"
#TARGETS="tl2"
#TARGETS="hytm2"
#TARGETS="hytm1"
TARGETS="seq tl2 hytm2sw hytm1 hytm2"
xflags1=-mx32
xflags2=-DNDEBUG

mkdir bin

g++ sum.cpp -o bin/sum -O3 -std=c++11
if [ "$?" -ne "0" ]; then
	echo "ERROR compiling sum.cpp"
	exit 1
fi

for t in ${TARGETS}
do
    # compile the TM library
    if [ $t == "seq" ]; then
        mfile="Makefile.seq"
    elif [ $t == "hytm2sw" ]; then
        cd hytm2
        make clean
        make EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2 EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=0
        if [ $? -ne 0 ]; then echo "ERROR: failed to compile TM library $t"; exit -1; fi
        cd ..
        echo "Compiled TM library: $t"
        mfile="Makefile.stm"
        mv hytm2/libhytm2.a hytm2sw/libhytm2sw.a
        cp hytm2/*.h hytm2sw/
    else
        cd $t
        make clean
        make EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2 EXTRAFLAGS3=$xflags3
        if [ $? -ne 0 ]; then echo "ERROR: failed to compile TM library $t"; exit -1; fi
        cd ..
        echo "Compiled TM library: $t"
        mfile="Makefile.stm"
    fi

    # compile the benchmarks that will use the library
    for p in ${PROGS}
    do
        cd $p
        make -f $mfile clean TARGET=$t
        make -f $mfile TARGET=$t EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2
        if [ $? -ne 0 ]; then "ERROR: failed to compile benchmark $p for TM library $t"; exit -1; fi
        mv ${p}_${t} ../bin/
        cd ..
        echo "Compiled benchmark $p for TM library $t"
    done
done
echo "Compiled all benchmarks successfully (I think...)"
