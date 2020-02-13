#!/bin/bash

cd ..
if [ "$#" -ne "0" ]; then
    echo "args: $@"
    make -j -f Makefile.stm machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20 $@
else
    make -j -f Makefile.stm machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20
fi
