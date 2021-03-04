#!/bin/bash

cd ..
if [ "$#" -ne "0" ]; then
    echo "args: $@"
    make -j $@
else
    make -j
fi
