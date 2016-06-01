/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef POOL_PERTHREAD_AND_SHARED_H
#define	POOL_PERTHREAD_AND_SHARED_H

#include <cassert>
#include <iostream>
#include "blockbag.h"
#include "blockpool.h"
#include "pool_interface.h"
#include "machineconstants.h"
#include "globals.h"
using namespace std;

#define POOL_THRESHOLD_IN_BLOCKS 4

template <typename T = void, class Alloc = allocator_interface<T> >
class pool_perthread_and_shared : public pool_interface<T, Alloc> {
private:
    lockfreeblockbag<T> *sharedBag;       // shared bag that we offload blocks on when we have too many in our freeBag
    blockbag<T> **freeBag;                // freeBag[tid] = bag of objects of type T that are ready to be reused by the thread with id tid

    // note: only does something if freeBag contains at least two full blocks
    inline bool tryGiveFreeObjects(const int tid) {
        if (freeBag[tid]->getSizeInBlocks() >= POOL_THRESHOLD_IN_BLOCKS) {
            block<T> *b = freeBag[tid]->removeFullBlock(); // returns NULL if freeBag has < 2 full blocks
            assert(b);
//            if (b) {
                sharedBag->addBlock(b);
                DS_DEBUG this->__debug->addGiven(tid, 1);
                //DS_DEBUG2 COUTATOMIC("  thread "<<this->tid<<" sharedBag("<<(sizeof(T)==sizeof(Node<long,long>)?"Node":"SCXRecord")<<") now contains "<<sharedBag->size()<<" blocks"<<endl);
//            }
            return true;
        }
        return false;
    }
//    
//    inline void tryTakeFreeObjects(const int tid) {
//        block<T> *b = sharedBag->getBlock();
//        if (b) {
//            freeBag[tid]->addFullBlock(b);
//            DS_DEBUG this->__debug->addTaken(tid, 1);
//            //DS_DEBUG2 COUTATOMIC("  thread "<<this->tid<<" took "<<b->computeSize()<<" objects from sharedBag"<<endl);
//        }
//    }
public:
    template<typename _Tp1>
    struct rebind {
        typedef pool_perthread_and_shared<_Tp1, Alloc> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef pool_perthread_and_shared<_Tp1, _Tp2> other;
    };
    
    /**
     * if the freebag contains any object, then remove one from the freebag
     * and return a pointer to it.
     * if not, then retrieve a new object from Alloc
     */
    inline T* get(const int tid) {
        return freeBag[tid]->template remove<Alloc>(tid, sharedBag, this->alloc);
    }
    inline void add(const int tid, T* ptr) {
        DS_DEBUG this->__debug->addToPool(tid, 1);
        freeBag[tid]->add(ptr, sharedBag, POOL_THRESHOLD_IN_BLOCKS);
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag, block<T> * const predecessor) {
        // WARNING: THE FOLLOWING DS_DEBUG COMPUTATION GETS THE WRONG NUMBER OF BLOCKS.
        DS_DEBUG this->__debug->addToPool(tid, (bag->getSizeInBlocks()-1)*BLOCK_SIZE);
        freeBag[tid]->appendMoveFullBlocks(bag, predecessor);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline void addMoveFullBlocks(const int tid, blockbag<T> *bag) {
        // WARNING: THE FOLLOWING DS_DEBUG COMPUTATION GETS THE WRONG NUMBER OF BLOCKS.
        DS_DEBUG this->__debug->addToPool(tid, (bag->getSizeInBlocks()-1)*BLOCK_SIZE);
        freeBag[tid]->appendMoveFullBlocks(bag);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline void addMoveAll(const int tid, blockbag<T> *bag) {
        DS_DEBUG this->__debug->addToPool(tid, bag->computeSize());
        freeBag[tid]->appendMoveAll(bag);
        while (tryGiveFreeObjects(tid)) {}
    }
    inline int computeSize(const int tid) {
        return freeBag[tid]->computeSize();
    }
    
    void debugPrintStatus(const int tid) {
        long free = computeSize(tid);
        long share = sharedBag->sizeInBlocks();
        COUTATOMIC("free="<<free<<" share="<<share);
    }
    
    pool_perthread_and_shared(const int numProcesses, Alloc * const _alloc, debugInfo * const _debug)
            : pool_interface<T, Alloc>(numProcesses, _alloc, _debug) {
        VERBOSE DS_DEBUG COUTATOMIC("constructor pool_perthread_and_shared"<<endl);
        freeBag = new blockbag<T>*[numProcesses];
        for (int tid=0;tid<numProcesses;++tid) {
            freeBag[tid] = new blockbag<T>(this->blockpools[tid]);
        }
        sharedBag = new lockfreeblockbag<T>();
    }
    ~pool_perthread_and_shared() {
        VERBOSE DS_DEBUG COUTATOMIC("destructor pool_perthread_and_shared"<<endl);
        // clean up shared bag
        const int dummyTid = 0;
        block<T> *fullBlock;
        while ((fullBlock = sharedBag->getBlock()) != NULL) {
            while (!fullBlock->isEmpty()) {
                T * const ptr = fullBlock->pop();
                this->alloc->deallocate(dummyTid, ptr);
            }
            this->blockpools[dummyTid]->deallocateBlock(fullBlock);
        }
        // clean up free bags
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            this->alloc->deallocateAndClear(tid, freeBag[tid]);
            delete freeBag[tid];
        }
        delete[] freeBag;
        delete sharedBag;
    }
};

#endif

