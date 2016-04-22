#!/bin/bash

g++ hytm2.cpp testing.cpp tmalloc.o -lpthread -o testing.out
if [ $? -eq 0 ]; then
    ./testing.out
else
    echo "Error during compilation!"
fi