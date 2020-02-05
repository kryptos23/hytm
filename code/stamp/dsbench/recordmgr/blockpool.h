/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef BLOCKPOOL_H
#define	BLOCKPOOL_H

#include "blockbag.h"
#include "machineconstants.h"
#include <iostream>
using namespace std;

#define MAX_BLOCK_POOL_SIZE 16

template <typename T>
class block;

template <typename T>
class blockpool {
private:
    PAD;
    block<T> *pool[MAX_BLOCK_POOL_SIZE];
    int poolSize;

    long debugAllocated;
    long debugPoolDeallocated;
    long debugPoolAllocated;
    long debugFreed;
    PAD;
public:
    blockpool() {
        poolSize = 0;
        debugAllocated = 0;
        debugPoolAllocated = 0;
        debugPoolDeallocated = 0;
        debugFreed = 0;
    }
    ~blockpool() {
        VERBOSE DS_DEBUG cout<<"destructor blockpool;";
        for (int i=0;i<poolSize;++i) {
            DS_DEBUG ++debugFreed;
            assert(pool[i]->isEmpty());
            delete pool[i];                           // warning: uses locks
        }
        VERBOSE DS_DEBUG cout<<" blocks allocated "<<debugAllocated<<" pool-allocated "<<debugPoolAllocated<<" freed "<<debugFreed<<" pool-deallocated "<<debugPoolDeallocated<<endl;
    }
    block<T>* allocateBlock(block<T> * const next) {
        if (poolSize) {
            DS_DEBUG ++debugPoolAllocated;
            block<T> *result = pool[--poolSize]; // pop a block off the stack
            *result = block<T>(next);
            assert(result->next == next);
            assert(result->computeSize() == 0);
            assert(result->isEmpty());
            return result;
        } else {
            DS_DEBUG ++debugAllocated;
            return new block<T>(next);                // warning: uses locks
        }
    }
    void deallocateBlock(block<T> * const b) {
        assert(b->isEmpty());
        if (poolSize == MAX_BLOCK_POOL_SIZE) {
            DS_DEBUG ++debugFreed;
            delete b;                                 // warning: uses locks
        } else {
            DS_DEBUG ++debugPoolDeallocated;
            pool[poolSize++] = b;
        }
    }
};

#endif	/* BLOCKPOOL_H */

