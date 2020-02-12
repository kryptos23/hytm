#!/bin/bash

cd ..
if [ "$#" -ne "0" ]; then
    echo "pinning=$1"
    make -j -f Makefile.stm machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20 pinning="$1"
else
    make -j -f Makefile.stm machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20
fi
