#!/bin/bash

#TARGETS="hytm2 hytm3 hybridnorec tl2"
#TARGETS="tl2 hytm2sw hytm1 hytm2"
#PROGS="dsbench"

#TARGETS="seqtm tl2 hytm1 hytm2 hytm3 hybridnorec"
#PROGS="dsbench bayes genome intruder kmeans labyrinth ssca2 vacation yada"

#TARGETS="seqtm tle tl2 hytm1 hytm2 hytm3 hybridnorec"
#TARGETS="tl2 hytm1 hytm2 hytm3 hybridnorec"
#TARGETS="hytm1 hytm2 hytm3 hybridnorec"
TARGETS="tl2"
PROGS="dsbench"

#TARGETS="hytm3 hytm2"
#TARGETS="tl2"
#TARGETS="hytm1 hytm2 hytm3 hybridnorec"
#TARGETS="hytm1 hytm2 hytm3 hybridnorec"
#TARGETS="tl2 hytm2sw hytm1 hytm2"
#PROGS="vacation yada"
#xflags2=-DNDEBUG

echo "Precompilation work..."
mkdir bin

g++ sum.cpp -o bin/sum -O3 -std=c++11
if [ "$?" -ne "0" ]; then
	echo "ERROR compiling sum.cpp"
	exit 1
fi

echo "Done."
echo

for t in ${TARGETS}
do
    # compile the TM library
    xflags3=""
    if [ $t == "seq" ]; then
        mfile="Makefile.seq"
    elif [ $t == "tle" ]; then
        mfile="Makefile.tle"
    elif [ $t == "hytm2sw" ]; then
        cd hytm2
        echo "Compiling TM library: $t"
        xflags3="-DHTM_ATTEMPT_THRESH=-1"
        make -f Makefile${makefileSuffix} clean
        make -f Makefile${makefileSuffix} EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2 EXTRAFLAGS3=$xflags3
        if [ $? -ne 0 ]; then echo "ERROR: failed to compile TM library $t"; exit -1; fi
        cd ..
        echo "Success."
        echo
        mfile="Makefile.stm"
        mv hytm2/libhytm2.a hytm2sw/libhytm2sw.a
        cp hytm2/*.h hytm2sw/
    else
        xflags3="-DHTM_ATTEMPT_THRESH=20"
#        xflags3="-DHTM_ATTEMPT_THRESH=-1"
        cd $t
        echo "Compiling TM library: $t"
        make -f Makefile${makefileSuffix} clean
        make -f Makefile${makefileSuffix} EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2 EXTRAFLAGS3=$xflags3
        if [ $? -ne 0 ]; then echo "ERROR: failed to compile TM library $t"; exit -1; fi
        cd ..
        echo "Success."
        echo
        mfile="Makefile.stm"
    fi

    # compile the benchmarks that will use the library
    for p in ${PROGS}
    do
        cd $p
        echo "Compiling benchmark $p for TM library $t"
        if [ -e $mfile ]; then
            make -f $mfile clean TARGET=$t
            make -f $mfile TARGET=$t EXTRAFLAGS1=$xflags1 EXTRAFLAGS2=$xflags2 EXTRAFLAGS3=$xflags3
            if [ $? -ne 0 ]; then
                echo "ERROR: failed to compile benchmark $p for TM library $t"
                exit 1
            fi
            mv ${p}_${t} ../bin/
            echo "Success. Moved ''./${p}_${t}'' to ''../bin/''."
        else
            echo "WARNING: no file ''$mfile'' to compile for target $t (may simply indicate an invalid pairing of stm library and benchmark that was left in the FOR loops for convenience)"
        fi
        echo
        cd ..
    done
done
echo "Compiled all benchmarks successfully (I think...)"
