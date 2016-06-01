#!/bin/sh

cd ~/.netbeans/remote/localhost/trbot-pc-Windows-x86_64/C/OneDrive/synced-documents/bitbucket/3path/paper.working/code/abtree/abtree
valgrind ./test_abtree
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
valgrind ./test_randomized validateEveryTime 1 keyRange 128 nops 1000000 printInterval 100000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized validateEveryTime 1 keyRange 16 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized validateEveryTime 1 keyRange 32 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized validateEveryTime 1 keyRange 128 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 16 nops 10000000 printInterval 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 32 nops 10000000 printInterval 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 128 nops 10000000 printInterval 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 1024 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 65536 nops 10000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 1048576 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
./test_randomized keyRange 1000000000 nops 1000000
if [ "$?" -ne "0" ]; then echo "Error." ; exit 1 ; fi
