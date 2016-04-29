#!/bin/bash

g++ -g -rdynamic hytm2.cpp testing.cpp tmalloc.o ../murmurhash/MurmurHash3.cpp -o testing.out -lpthread -ltcmalloc
if [ $? -eq 0 ]; then
    ./testing.out
else
    echo "Error during compilation!"
fi
