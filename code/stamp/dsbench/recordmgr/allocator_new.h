/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef ALLOC_NEW_H
#define	ALLOC_NEW_H

#include "machineconstants.h"
#include "pool_interface.h"
#include <cstdlib>
#include <cassert>
#include <iostream>
using namespace std;

template<typename T = void>
class allocator_new : public allocator_interface<T> {
public:
    template<typename _Tp1>
    struct rebind {
        typedef allocator_new<_Tp1> other;
    };
    
    // reserve space for ONE object of type T
    T* allocate(const int tid) {
//        // first, try to get an object from the freeBag
//        T* result = this->pool->get(tid);
//        if (result) return result;
        // allocate a new object
        DS_DEBUG {
            this->__debug->addAllocated(tid, 1);
            VERBOSE {
                if ((this->__debug->getAllocated(tid) % 2000) == 0) {
                    debugPrintStatus(tid);
                }
            }
        }
        return new T; //(T*) malloc(sizeof(T));
    }
    void deallocate(const int tid, T * const p) {
        delete p;
//        this->pool->add(tid, p);
    }
    void deallocateAndClear(const int tid, blockbag<T> * const bag) {
        while (!bag->isEmpty()) {
            T* ptr = bag->remove();
            deallocate(tid, ptr);
        }
//        this->pool->addAndClear(tid, bag);
    }
    
    void debugPrintStatus(const int tid) {
        cout<<"thread "<<tid<<" allocated "<<this->__debug->getAllocated(tid)<<" objects of size "<<(sizeof(T));
        cout<<" ";
//        this->pool->debugPrintStatus(tid);
        cout<<endl;
    }
    
    void initThread(const int tid) {}
    
    allocator_new(const int numProcesses, debugInfo * const _debug)
            : allocator_interface<T>(numProcesses, _debug) {
        VERBOSE DS_DEBUG cout<<"constructor allocator_new"<<endl;
    }
    ~allocator_new() {
        VERBOSE DS_DEBUG cout<<"destructor allocator_new"<<endl;
    }
};

#endif	/* ALLOC_NEW_H */

