/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef ALLOC_INTERFACE_H
#define	ALLOC_INTERFACE_H

#include "debug_info.h"
#include "blockbag.h"
#include <iostream>
using namespace std;

template <typename T = void>
class allocator_interface {
public:
    debugInfo * const __debug;
    
    const int NUM_PROCESSES;
    
    template<typename _Tp1>
    struct rebind {
        typedef allocator_interface<_Tp1> other;
    };
    
    // allocate space for one object of type T
    T* allocate(const int tid);
    void deallocate(const int tid, T * const p);
    void deallocateAndClear(const int tid, blockbag<T> * const bag);
    void initThread(const int tid);
    
    void debugPrintStatus(const int tid);

    allocator_interface(const int numProcesses, debugInfo * const _debug)
            : NUM_PROCESSES(numProcesses), __debug(_debug) {
        VERBOSE DS_DEBUG cout<<"constructor allocator_interface"<<endl;
    }
    ~allocator_interface() {
        VERBOSE DS_DEBUG cout<<"destructor allocator_interface"<<endl;
    }
};

#endif
