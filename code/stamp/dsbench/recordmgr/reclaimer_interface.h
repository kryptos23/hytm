/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef RECLAIM_INTERFACE_H
#define	RECLAIM_INTERFACE_H

#include "recovery_manager.h"
#include "pool_interface.h"
#include "globals.h"
#include <iostream>
#include <cstdlib>
using namespace std;

template <typename T = void, class Pool = pool_interface<T> >
class reclaimer_interface {
public:
    RecoveryMgr<void *> * recoveryMgr;
    debugInfo * const __debug;
    
    const int NUM_PROCESSES;
    Pool *pool;

    template<typename _Tp1>
    struct rebind {
        typedef reclaimer_interface<_Tp1, Pool> other;
    };
    template<typename _Tp1, typename _Tp2>
    struct rebind2 {
        typedef reclaimer_interface<_Tp1, _Tp2> other;
    };

    inline static bool quiescenceIsPerRecordType() { return true; }
    inline static bool shouldHelp() { return true; } // FOR DEBUGGING PURPOSES
    inline static bool supportsCrashRecovery() { return false; }
    inline bool isProtected(const int tid, T * const obj);
    inline bool isQProtected(const int tid, T * const obj);
    inline static bool isQuiescent(const int tid) {
        COUTATOMICTID("reclaimer_interface::isQuiescent(tid) is not implemented!"<<endl);
        exit(-1);
    }
    
    // for hazard pointers (and reference counting)
    inline bool protect(const int tid, T* obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true);
    inline void unprotect(const int tid, T* obj);
    inline bool qProtect(const int tid, T* obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool memoryBarrier = true);
    inline void qUnprotectAll(const int tid);
    
    // for epoch based reclamation (or, more generally, any quiescent state based reclamation)
//    inline long readEpoch();
//    inline long readAnnouncedEpoch(const int tid);
    /**
     * enterQuiescentState<T> must be idempotent,
     * and must unprotect all objects protected by calls to protectObject<T>.
     * it must NOT unprotect any object protected by a call to
     * protectObjectEvenAfterRestart.
     */
    inline void enterQuiescentState(const int tid);
    inline bool leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers);
    inline void rotateEpochBags(const int tid);

    // for all schemes except reference counting
    inline void retire(const int tid, T* p);
    
    void debugPrintStatus(const int tid);
    
    reclaimer_interface(const int numProcesses, Pool *_pool, debugInfo * const _debug, RecoveryMgr<void *> * const _recoveryMgr = NULL)
            : NUM_PROCESSES(numProcesses), pool(_pool), __debug(_debug), recoveryMgr(_recoveryMgr) {
        VERBOSE DS_DEBUG COUTATOMIC("constructor reclaimer_interface"<<endl);
    }
    ~reclaimer_interface() {
        VERBOSE DS_DEBUG COUTATOMIC("destructor reclaimer_interface"<<endl);
    }
};

#endif
