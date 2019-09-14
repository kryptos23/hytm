#!/bin/bash

g++ -g -O3 -pg hytm2.cpp testing.cpp tmalloc.o -o testing.out -lpthread -ltcmalloc
if [ $? -eq 0 ]; then
    ./testing.out
else
    echo "Error during compilation!"
fi
