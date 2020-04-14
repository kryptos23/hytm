/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 *
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef RECLAIM_EPOCH_H
#define	RECLAIM_EPOCH_H

#include <cassert>
#include <iostream>
#include "blockbag.h"
#include "machineconstants.h"
#include "allocator_interface.h"
#include "reclaimer_interface.h"
using namespace std;

#ifndef LWSYNC
#if defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#   define LWSYNC asm volatile("lwsync" ::: "memory")
#   define SYNC asm volatile("sync" ::: "memory")
#elif defined(__x86_64__) || defined(_M_X64)
#   define LWSYNC /* nothing */
#   define SYNC __sync_synchronize()
#else
#   error UNKNOWN platform
#endif
#endif

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_debra : public reclaimer_interface<T, Pool> {
protected:
#define EPOCH_INCREMENT 2
#define BITS_EPOCH(ann) ((ann)&~(EPOCH_INCREMENT-1))
#define QUIESCENT(ann) ((ann)&1)
#define GET_WITH_QUIESCENT(ann) ((ann)|1)

#define MINIMUM_OPERATIONS_BEFORE_NEW_EPOCH 100
#define NUMBER_OF_EPOCH_BAGS 3
#define NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS 10

// #define TECHNIQUE_QBITS
#define TECHNIQUE_QWORDS


    // for epoch based reclamation
    PAD;
    volatile long epoch;
    PAD;
    atomic_long * announcedEpoch;       // announcedEpoch[tid*PREFETCH_SIZE_WORDS]
#ifdef TECHNIQUE_QWORDS
    atomic_long * quiescence;
#endif
#ifdef TECHNIQUE_QBITS
    long * checked;                     // checked[tid*PREFETCH_SIZE_WORDS] = how far we've come in checking the announced epochs of other threads
#endif
    blockbag<T> ** epochbags;           // epochbags[NUMBER_OF_EPOCH_BAGS*tid+0..NUMBER_OF_EPOCH_BAGS*tid+(NUMBER_OF_EPOCH_BAGS-1)] are epoch bags for thread tid.
    blockbag<T> ** currentBag;          // pointer to current epoch bag for each process
    long * index;                       // index of currentBag in epochbags for each process
    PAD;
    // note: oldest bag is number (index+1)%NUMBER_OF_EPOCH_BAGS

public:
    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_debra<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_debra<_Tp1, _Tp2> other;
    };

    inline static bool quiescenceIsPerRecordType() { return false; }

    inline bool isQuiescent(const int tid) {
#ifdef TECHINQUE_QBITS
        return QUIESCENT(announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed));
#elif defined TECHNIQUE_QWORDS
        return true;
#endif
    }

    inline static bool isProtected(const int tid, T * const obj) {
        return true;
    }
    inline static bool isQProtected(const int tid, T * const obj) {
        return false;
    }
    inline static bool protect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void unprotect(const int tid, T * const obj) {}
    inline static bool qProtect(const int tid, T * const obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true) {
        return true;
    }
    inline static void qUnprotectAll(const int tid) {}

    // rotate the epoch bags and reclaim any objects retired two epochs ago.
    inline void rotateEpochBags(const int tid) {
        index[tid*PREFETCH_SIZE_WORDS] = (index[tid*PREFETCH_SIZE_WORDS]+1) % NUMBER_OF_EPOCH_BAGS;
        blockbag<T> * const freeable = epochbags[NUMBER_OF_EPOCH_BAGS*tid+index[tid*PREFETCH_SIZE_WORDS]];
        this->pool->addMoveFullBlocks(tid, freeable); // moves any full blocks (may leave a non-full block behind)
        currentBag[tid*PREFETCH_SIZE_WORDS] = freeable;
    }
    // invoke this at the beginning of each operation that accesses
    // objects reclaimed by this epoch manager.
    // returns true if the call rotated the epoch bags for thread tid
    // (and reclaimed any objects retired two epochs ago).
    // otherwise, the call returns false.
    inline bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        static thread_local int numCalls = 0;
        ++numCalls;

#ifdef TECHNIQUE_QWORDS
        static thread_local int checked = 0;
        static thread_local int otherQuiescence = -1;
        static thread_local int consecutiveFailedChecks = 0;
#endif

        //SOFTWARE_BARRIER; // prevent any bookkeeping from being moved after this point by the compiler.
        long readEpoch = epoch;

#ifdef TECHINQUE_QBITS
        bool result = false;

        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        // if our announced epoch is different from the current epoch
        if (readEpoch != BITS_EPOCH(ann)) {
            // announce the new epoch, and rotate the epoch bags and
            // reclaim any objects retired two epochs ago.
            numCalls = 1;
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
            //rotateEpochBags(tid);
            for (int i=0;i<numReclaimers;++i) {
                ((reclaimer_debra<T, Pool> * const) reclaimers[i])->rotateEpochBags(tid);
            }
            result = true;
        }
        // note: readEpoch, when written to announcedEpoch[tid],
        //       will set the state to non-quiescent and non-neutralized

        // incrementally scan the announced epochs of all threads
        // (to avoid high NUMA-caused L3 miss costs only participate in this part of the alg once every X calls to this function)
        if (0 == (numCalls % NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS)) {
            int otherTid = checked[tid*PREFETCH_SIZE_WORDS];
            if (otherTid >= this->NUM_PROCESSES) {
                const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
                if (NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS*c > MINIMUM_OPERATIONS_BEFORE_NEW_EPOCH) {
                    // note: __sync functions imply membars in gcc (for power)
                    __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
                    // note: __sync functions imply membars in gcc (for power)
                }
            } else {
                assert(otherTid >= 0);
                long otherAnnounce = announcedEpoch[otherTid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
                if (BITS_EPOCH(otherAnnounce) == readEpoch
                        || QUIESCENT(otherAnnounce)) {
                    const int c = ++checked[tid*PREFETCH_SIZE_WORDS];
                    if (c >= this->NUM_PROCESSES && NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS*c > MINIMUM_OPERATIONS_BEFORE_NEW_EPOCH) {
                        // note: __sync functions imply membars in gcc (for power)
                        __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+EPOCH_INCREMENT);
                        // note: __sync functions imply membars in gcc (for power)
                    }
                }
            }
        }
        // SOFTWARE_BARRIER;
        LWSYNC; // prevent announcement from being moved earlier (on power) [is this actually necessary?]
        announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(readEpoch, memory_order_relaxed);
        // note: SYNC is not needed here, since a full membar is placed just after this return statement by the record manager (for power)
        return result;

#elif defined TECHNIQUE_QWORDS

        auto a = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        if (a != readEpoch) {
            // SOFTWARE_BARRIER;
            numCalls = 1;
            checked = 0;

            otherQuiescence = -1;
            consecutiveFailedChecks = 0;

            for (int i=0;i<numReclaimers;++i) {
                ((reclaimer_debra<T, Pool> * const) reclaimers[i])->rotateEpochBags(tid);
            }
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(readEpoch, memory_order_relaxed);
            // printf("TRACE: tid=%d rotates bags with a=%ld and readEpoch=%ld\n", tid, a, readEpoch);
            return true;

        } else {
            // if (tid == 0) printf("TRACE: tid=%d did saw a==readEpoch; a=%ld readEpoch=%ld checked=%d NUM_PROCESSES=%d\n", tid, a, readEpoch, checked, this->NUM_PROCESSES);
            if (0 == (numCalls % NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS)) {
                if (checked < this->NUM_PROCESSES) {
                    auto aOther = announcedEpoch[checked*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
                    if (aOther >= readEpoch) {
                        ++checked;
                        otherQuiescence = -1;
                        consecutiveFailedChecks = 0;
                    } else {
                        if (++consecutiveFailedChecks > this->NUM_PROCESSES / NUM_CALLS_BETWEEN_ANNOUNCEMENT_CHECKS) {
                            auto qOther = quiescence[checked*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
                            if (otherQuiescence == -1) {
                                otherQuiescence = qOther;
                            } else {
                                if (otherQuiescence != qOther) {
                                    ++checked;
                                    otherQuiescence = -1;
                                    consecutiveFailedChecks = 0;
                                }
                            }
                        }
                    }
                }
                if (checked >= this->NUM_PROCESSES) {
                    auto result = __sync_bool_compare_and_swap(&epoch, readEpoch, readEpoch+1);
                    // if (result) printf("TRACE: tid=%d calls CAS from %ld and received %d\n", tid, readEpoch, result);
                }
            }
        }
        return false;
#endif
    }
    inline void enterQuiescentState(const int tid) {
#ifdef TECHNIQUE_QBITS
        const long ann = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(ann), memory_order_relaxed);
#elif defined TECHNIQUE_QWORDS
        const long q = quiescence[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        quiescence[tid*PREFETCH_SIZE_WORDS].store(1+q, memory_order_relaxed);
#endif
    }

    // for all schemes except reference counting
    inline void retire(const int tid, T* p) {
        currentBag[tid*PREFETCH_SIZE_WORDS]->add(p);
        DS_DEBUG this->__debug->addRetired(tid, 1);
    }

    void debugPrintStatus(const int tid) {
        assert(tid >= 0);
        assert(tid < this->NUM_PROCESSES);
        long announce = announcedEpoch[tid*PREFETCH_SIZE_WORDS].load(memory_order_relaxed);
        cout<<"announce="<<announce<<" bags:";
        for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
            cout<<" bag"<<i<<"="<<epochbags[NUMBER_OF_EPOCH_BAGS*tid+i]->computeSize();
        }
    }

    reclaimer_debra(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : reclaimer_interface<T, Pool>(numProcesses, _pool, _debug, _recoveryMgr) {
        VERBOSE cout<<"constructor reclaimer_debra helping="<<this->shouldHelp()<<endl;// scanThreshold="<<scanThreshold<<endl;
        epoch = 0;
        epochbags = new blockbag<T>*[NUMBER_OF_EPOCH_BAGS*MAX_TID_POW2 + PREFETCH_SIZE_WORDS*2] + PREFETCH_SIZE_WORDS; // shift to pad
        currentBag = new blockbag<T>*[(2+MAX_TID_POW2)*PREFETCH_SIZE_WORDS] + PREFETCH_SIZE_WORDS;      /* shift to pad */
        index = new long[(2+MAX_TID_POW2)*PREFETCH_SIZE_WORDS] + PREFETCH_SIZE_WORDS;                   /* shift to pad */
        announcedEpoch = new atomic_long[(2+MAX_TID_POW2)*PREFETCH_SIZE_WORDS] + PREFETCH_SIZE_WORDS;   /* shift to pad */
#ifdef TECHNIQUE_QWORDS
        quiescence = new atomic_long[(2+MAX_TID_POW2)*PREFETCH_SIZE_WORDS] + PREFETCH_SIZE_WORDS;       /* shift to pad */
#endif
#ifdef TECHNIQUE_QBITS
        checked = new long[(2+MAX_TID_POW2)*PREFETCH_SIZE_WORDS] + PREFETCH_SIZE_WORDS;                 /* shift to pad */
#endif
        for (int tid=0;tid<MAX_TID_POW2;++tid) {
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
                epochbags[NUMBER_OF_EPOCH_BAGS*tid+i] = new blockbag<T>(this->pool->blockpools[tid]);
            }
            currentBag[tid*PREFETCH_SIZE_WORDS] = epochbags[NUMBER_OF_EPOCH_BAGS*tid];
            index[tid*PREFETCH_SIZE_WORDS] = 0;
#ifdef TECHNIQUE_QWORDS
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(0, memory_order_relaxed);
            quiescence[tid*PREFETCH_SIZE_WORDS].store(0, memory_order_relaxed);
#endif
#ifdef TECHNIQUE_QBITS
            announcedEpoch[tid*PREFETCH_SIZE_WORDS].store(GET_WITH_QUIESCENT(0), memory_order_relaxed);
            checked[tid*PREFETCH_SIZE_WORDS] = 0;
#endif
        }
    }
    ~reclaimer_debra() {
        VERBOSE DS_DEBUG cout<<"destructor reclaimer_debra"<<endl;
        cout<<"reclaimer_debra epoch="<<epoch<<endl;
        for (int tid=0;tid<MAX_TID_POW2;++tid) {
            // move contents of all bags into pool
            for (int i=0;i<NUMBER_OF_EPOCH_BAGS;++i) {
                this->pool->addMoveAll(tid, epochbags[NUMBER_OF_EPOCH_BAGS*tid+i]);
                delete epochbags[NUMBER_OF_EPOCH_BAGS*tid+i];
            }
        }
        delete[] (epochbags - PREFETCH_SIZE_WORDS);         /* unshift to remove padding */
        delete[] (index - PREFETCH_SIZE_WORDS);             /* unshift to remove padding */
        delete[] (currentBag - PREFETCH_SIZE_WORDS);        /* unshift to remove padding */
        delete[] (announcedEpoch - PREFETCH_SIZE_WORDS);    /* unshift to remove padding */
#ifdef TECHNIQUE_QWORDS
        delete[] (quiescence - PREFETCH_SIZE_WORDS);        /* unshift to remove padding */
#endif
#ifdef TECHNIQUE_QBITS
        delete[] (checked - PREFETCH_SIZE_WORDS);           /* unshift to remove padding */
#endif
    }

};

#endif

