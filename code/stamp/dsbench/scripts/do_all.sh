#!/bin/bash

dir=$(pwd)
outdir=$dir/../output.$(hostname).dsbench/png

if [ "$#" -eq "1" ]; then
    testing="testing"
else
    testing=""
fi

./dsbench $testing && \
    ./dsbench.format.parallel $(hostname) && \
    ./_create_graphs.sh && \
    cp gen_html/* $outdir/ && \
    cd $outdir && ./_gen.sh && \
    rsync -r -p --delete . linux.cs.uwaterloo.ca:public_html/rift/preview/hytm/

if [ "$?" -ne "0" ]; then
    echo "ERROR in do_all.sh"
fi
