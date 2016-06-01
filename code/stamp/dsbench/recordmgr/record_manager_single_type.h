/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef RECORD_MANAGER_SINGLE_TYPE_H
#define	RECORD_MANAGER_SINGLE_TYPE_H

#include <pthread.h>
#include <setjmp.h>
#include <errno.h>
#include <cstring>
#include <iostream>

#include "machineconstants.h"
#include "debug_info.h"
#include "globals.h"

#include "allocator_interface.h"
#include "allocator_bump.h"
#include "allocator_new.h"
#include "allocator_once.h"

#include "pool_interface.h"
#include "pool_none.h"
#include "pool_perthread_and_shared.h"

#include "reclaimer_interface.h"
#include "reclaimer_none.h"
#include "reclaimer_debra.h"
#include "reclaimer_debraplus.h"
#include "reclaimer_hazardptr.h"
#include "recovery_manager.h"

using namespace std;

// maybe Record should be a size
template <typename Record, class Reclaim, class Alloc, class Pool>
class record_manager_single_type {
protected:
    typedef Record* record_pointer;

    typedef typename Alloc::template    rebind<Record>::other              classAlloc;
    typedef typename Pool::template     rebind2<Record, classAlloc>::other classPool;
    typedef typename Reclaim::template  rebind2<Record, classPool>::other  classReclaim;
    
public:
    classAlloc      *alloc;
    classPool       *pool;
    classReclaim    *reclaim;
    
    const int NUM_PROCESSES;
    debugInfo debugInfoRecord;
    RecoveryMgr<void *> * const recoveryMgr;

    record_manager_single_type(const int numProcesses, RecoveryMgr<void *> * const _recoveryMgr)
            : NUM_PROCESSES(numProcesses), debugInfoRecord(debugInfo(numProcesses)), recoveryMgr(_recoveryMgr) {
        VERBOSE DS_DEBUG COUTATOMIC("constructor record_manager_single_type"<<endl);
        alloc = new classAlloc(numProcesses, &debugInfoRecord);
        pool = new classPool(numProcesses, alloc, &debugInfoRecord);
        reclaim = new classReclaim(numProcesses, pool, &debugInfoRecord, recoveryMgr);
    }
    ~record_manager_single_type() {
        VERBOSE DS_DEBUG COUTATOMIC("destructor record_manager_single_type"<<endl);
        delete reclaim;
        delete pool;
        delete alloc;
    }

    void initThread(const int tid) {
        alloc->initThread(tid);
//        enterQuiescentState(tid);
    }
    
    inline void clearCounters() {
        debugInfoRecord.clear();
    }

    inline static bool shouldHelp() { // FOR DEBUGGING PURPOSES
        return Reclaim::shouldHelp();
    }
    inline bool isProtected(const int tid, record_pointer obj) {
        return reclaim->isProtected(tid, obj);
    }
    // for hazard pointers (and reference counting)
    inline bool protect(const int tid, record_pointer obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool hintMemoryBarrier = true) {
        return reclaim->protect(tid, obj, notRetiredCallback, callbackArg, hintMemoryBarrier);
    }
    inline void unprotect(const int tid, record_pointer obj) {
        reclaim->unprotect(tid, obj);
    }
    // warning: qProtect must be reentrant and lock-free (=== async-signal-safe)
    inline bool qProtect(const int tid, record_pointer obj, CallbackType notRetiredCallback, CallbackArg callbackArg, bool hintMemoryBarrier = true) {
        return reclaim->qProtect(tid, obj, notRetiredCallback, callbackArg, hintMemoryBarrier);
    }
    inline void qUnprotectAll(const int tid) {
        assert(!Reclaim::supportsCrashRecovery() || isQuiescent(tid));
        reclaim->qUnprotectAll(tid);
    }
    inline bool isQProtected(const int tid, record_pointer obj) {
        return reclaim->isQProtected(tid, obj);
    }
    
    inline static bool supportsCrashRecovery() {
        return Reclaim::supportsCrashRecovery();
    }
    inline static bool quiescenceIsPerRecordType() {
        return Reclaim::quiescenceIsPerRecordType();
    }
    inline bool isQuiescent(const int tid) {
        return reclaim->isQuiescent(tid);
    }

    // for epoch based reclamation
    inline void enterQuiescentState(const int tid) {
//        VERBOSE DS_DEBUG2 COUTATOMIC("record_manager_single_type::enterQuiescentState(tid="<<tid<<")"<<endl);
        reclaim->enterQuiescentState(tid);
    }
    inline void leaveQuiescentState(const int tid, void * const * const reclaimers, const int numReclaimers) {
        assert(isQuiescent(tid));
        reclaim->leaveQuiescentState(tid, reclaimers, numReclaimers);
    }

    // for all schemes except reference counting
    inline void retire(const int tid, record_pointer p) {
        assert(!Reclaim::supportsCrashRecovery() || isQuiescent(tid));
        reclaim->retire(tid, p);
    }

    // for all schemes
    inline record_pointer allocate(const int tid) {
        assert(!Reclaim::supportsCrashRecovery() || isQuiescent(tid));
        return pool->get(tid);
    }
    inline void deallocate(const int tid, record_pointer p) {
        assert(!Reclaim::supportsCrashRecovery() || isQuiescent(tid));
        pool->add(tid, p);
    }

    void printStatus(void) {
        for (int tid=0;tid<this->NUM_PROCESSES;++tid) {
            COUTATOMIC("thread "<<tid<<" ");
            alloc->debugPrintStatus(tid);
            COUTATOMIC(" ");
            COUTATOMIC("allocated "<<debugInfoRecord.getAllocated(tid)<<" Nodes");
            //COUTATOMIC("allocated "<<(debugInfoRecord.getAllocated(tid) / 1000)<<"k Nodes");
            COUTATOMIC(" ");
            reclaim->debugPrintStatus(tid);
            COUTATOMIC(" ");
            pool->debugPrintStatus(tid);
            COUTATOMIC(" ");
            COUTATOMIC("(given="<<debugInfoRecord.getGiven(tid)<<" taken="<<debugInfoRecord.getTaken(tid)<<") toPool="<<debugInfoRecord.getToPool(tid));
            COUTATOMIC(endl);
        }
    }
};

#endif