#!/bin/bash
#
# File:   plaf.bash
# Author: tabrown
#
# Created on 17-Jun-2016, 3:59:24 PM
#

machine=`hostname`
if [ "$machine" == "csl-pomela6" ]; then
    xargs1="-mx32"
    maxthreadcount="64"
    overthreadcount1="128"
    overthreadcount2="256"
    threadcounts="1 2 4 8 16 24 32 40 48 56 64"
    pin_scatter="POMELA6_SCATTER"
    pin_cluster="IDENTITY"
    pin_none="NONE"
    cmdprefix=""
elif [ "$machine" == "pomela3" ]; then
    xargs1="-mx32"
    maxthreadcount="64"
    overthreadcount1="128"
    overthreadcount2="256"
    threadcounts="1 2 4 8 16 24 32 40 48 56 64"
    pin_scatter="POMELA6_SCATTER"
    pin_cluster="IDENTITY"
    pin_none="NONE"
    cmdprefix=""
elif [ "$machine" == "tapuz40" ]; then
    xargs1="-mx32"
    maxthreadcount="48"
    overthreadcount1="96"
    overthreadcount2="192"
    threadcounts="1 2 4 8 12 16 20 24 32 40 48"
    pin_scatter="TAPUZ40_SCATTER"
    pin_cluster="TAPUZ40_CLUSTER"
    pin_none="NONE"
    cmdprefix=""
elif [ "$machine" == "theoryhtm" ]; then
    xargs1="-mx32"
    maxthreadcount="8"
    overthreadcount1="16"
    overthreadcount2="32"
    threadcounts="1 2 3 4 5 6 7 8"
    pin_scatter="IDENTITY"
    pin_cluster="IDENTITY"
    pin_none="NONE"
    cmdprefix=""
elif [ "$machine" == "cheshire-r07u03" ]; then
    xargs1=""
    maxthreadcount="192"
    overthreadcount1="192"
    overthreadcount2="192"
    threadcounts="1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 28 32 36 40 44 48"
    pin_scatter="SOSCIP_SCATTER"
    pin_cluster="SOSCIP_CLUSTER"
    pin_none="NONE"
    cmdprefix=""
else
    echo "ERROR: unknown machine $machine"
    exit 1
fi

g++ -O3 add.cpp -o add
