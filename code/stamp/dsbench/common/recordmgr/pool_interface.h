/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 *
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef POOL_INTERFACE_H
#define	POOL_INTERFACE_H

#include <iostream>
#include "allocator_interface.h"
#include "debug_info.h"
#include "blockpool.h"
#include "blockbag.h"
using namespace std;

template <typename T = void, class Alloc = allocator_interface<T> >
class pool_interface {
public:
    PAD;
    debugInfo * const __debug;

    const int NUM_PROCESSES;
    blockpool<T> **blockpools; // allocated (or not) and freed by descendants
    Alloc *alloc;
    PAD;

    template<typename _Tp1>
    struct rebind {
        typedef pool_interface<_Tp1, Alloc> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef pool_interface<_Tp1, _Tp2> other;
    };

    /**
     * if the pool contains any object, then remove one from the pool
     * and return a pointer to it. otherwise, return NULL.
     */
    inline T* get(const int tid);
    inline void add(const int tid, T* ptr);
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag);
    inline void addMoveAll(const int tid, blockbag<T> *bag);
    inline int computeSize(const int tid);

    void debugPrintStatus(const int tid);

    pool_interface(const int numProcesses, Alloc * const _alloc, debugInfo * const _debug)
            : NUM_PROCESSES(numProcesses), alloc(_alloc), __debug(_debug) {
        VERBOSE DS_DEBUG cout<<"constructor pool_interface"<<endl;
        this->blockpools = new blockpool<T>*[MAX_TID_POW2];
        for (int tid=0;tid<MAX_TID_POW2;++tid) {
            this->blockpools[tid] = new blockpool<T>();
        }
    }
    ~pool_interface() {
        VERBOSE DS_DEBUG cout<<"destructor pool_interface"<<endl;
        for (int tid=0;tid<MAX_TID_POW2;++tid) {
            delete this->blockpools[tid];
        }
        delete[] this->blockpools;
    }
};

#endif

