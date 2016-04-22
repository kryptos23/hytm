/**
 * Code for HyTM is loosely based on the code for TL2
 * (in particular, the data structures)
 * 
 * This is an implementation of Algorithm 1 from the paper.
 * 
 * [ note: we cannot distribute this without inserting the appropriate
 *         copyright notices as required by TL2 and STAMP ]
 * 
 * Authors: Trevor Brown (tabrown@cs.toronto.edu) and Srivatsan Ravi
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "platform.h"
#include "hytm2.h"
#include "tmalloc.h"
#include "util.h"
#include "vlock.h"
#include "avpair.h"
#include "thread.h"
#include "log.h"
#include "typelogs.h"

#define TM_NAME "HyTM2"
#define HTM_RETRY_THRESH 5

extern volatile long StartTally;
extern volatile long AbortTally;
//volatile long ReadOverflowTally  = 0;
//volatile long WriteOverflowTally = 0;
//volatile long LocalOverflowTally = 0;








//__INLINE__ unsigned long long MarsagliaXORV(unsigned long long x) {
//    if (x == 0) {
//        x = 1;
//    }
//    x ^= x << 6;
//    x ^= x >> 21;
//    x ^= x << 7;
//    return x;
//}
//
//__INLINE__ unsigned long long MarsagliaXOR(unsigned long long* seed) {
//    unsigned long long x = MarsagliaXORV(*seed);
//    *seed = x;
//    return x;
//}

vLock LockTab[_TABSZ];







/**
 * 
 * IMPLEMENTATION OF TM OPERATIONS
 * 
 */

void TxStart(void* _Self, sigjmp_buf* envPtr, int aborted_in_software, int* ROFlag) {
    Thread* Self = (Thread*) _Self;
    Self->wrSet->clear();
    Self->rdSet->clear();
    Self->LocalUndo->clear();

    Self->Retries = 0;
    Self->isFallback = 0;
//    Self->ROFlag = ROFlag;
    Self->IsRO = true;
    Self->envPtr = envPtr;
    
    unsigned status;
    if (aborted_in_software) {
        goto software;
    }
    Self->Starts++;
htmretry:
    status = XBEGIN();
    if (status == _XBEGIN_STARTED) {
        
    } else {
        // if we aborted
        ++Self->Aborts;
        if (++Self->Retries < HTM_RETRY_THRESH) {
            goto htmretry;
        } else {
            goto software;
        }
    }
    return;
software:
    Self->isFallback = 1;
}

int TxCommit(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        // return immediately if txn is read-only
        if (Self->IsRO) {
            return true;
        }
        
        // lock all addresses in the write-set
        if (!Self->wrSet->lockAll(Self)) {
            TxAbort(Self); // abort if we fail to acquire some lock
            // (note: after lockAll we hold either all or no locks)
        }
        
        // validate reads
        if (!Self->rdSet->validate(Self)) {
            // if we fail validation, release all locks and abort
            Self->wrSet->releaseAll(Self);
            TxAbort(Self);
        }
        
        // perform the actual writes
        Self->wrSet->writeForward();
        
        // release all locks
        Self->wrSet->releaseAll(Self);
        
        // deal with any malloc'd memory
        tmalloc_clear(Self->allocPtr);
        tmalloc_releaseAllForward(Self->freePtr, NULL);
        return true;
        
    // hardware path
    } else {
        tmalloc_releaseAllForward(Self->freePtr, NULL);
        XEND();
        tmalloc_clear(Self->allocPtr);
        return true;
    }
}

void TxAbort(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        ++Self->Retries;
        ++Self->Aborts;
        
        // Rollback any memory allocation, and longjmp to start of txn
        tmalloc_releaseAllReverse(Self->allocPtr, NULL);
        tmalloc_clear(Self->freePtr);
        SIGLONGJMP(*Self->envPtr, 1);
        ASSERT(0);
        
    // hardware path
    } else {
        XABORT(0);
    }
}

template <typename T>
T TxLoad(void* _Self, volatile T* Addr) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        vLock* lock = PSLOCK(Addr);
        AVPair* av = Self->rdSet->find<T>(lock); // TODO: improve find w/hashing or bloom filter
        if (av != NULL) { // Addr is in read-set
            return unpackValue<T>(av);
        }

        // Addr is NOT in read-set
        vLockSnapshot locksnap = lock->getSnapshot();
        T val = *Addr;
        Self->rdSet->append(Addr, val, lock, locksnap);

        // abort if Addr was locked when we got a snapshot of its lock
        if (locksnap.isLocked()) {
            TxAbort(Self);
        }
        
        // validate reads
        if (!Self->rdSet->validate(Self)) {
            TxAbort(Self);
        }
        
        return val;
        
    // hardware path
    } else {
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        T val = *Addr;
        
        // abort if Addr was locked when we got a snapshot of its lock
        if (locksnap.isLocked()) {
            TxAbort(Self);
        }
        return val;
    }
}

template <typename T>
T TxStore(void* _Self, volatile T* addr, T valu) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        vLock* lock = PSLOCK(addr);
        Self->wrSet->append<T>(addr, valu, lock, lock->getSnapshot());
        Self->IsRO = false; // txn is not read-only
        return valu;
        
    // hardware path
    } else {
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) {
            XABORT(_XABORT_EXPLICIT_LOCKED);
        }
        lock->htmIncrementVersion();
        return *addr = valu;
    }
}










/**
 * 
 * FRAMEWORK FUNCTIONS
 * (PROBABLY DON'T NEED TO BE CHANGED WHEN CREATING A VARIATION OF THIS TM)
 * 
 */

void TxOnce() {
    CTASSERT((_TABSZ & (_TABSZ - 1)) == 0); /* must be power of 2 */
    printf("%s %s\n", TM_NAME, "system ready\n");
}

void TxShutdown() {
    printf("%s %s:\n  Starts=%li Aborts=%li\n",
            TM_NAME, "system shutdown", StartTally, AbortTally);
}

void* TxNewThread() {
    Thread* t = (Thread*) malloc(sizeof (Thread));
    assert(t);
    return t;
}

void TxFreeThread(void* _t) {
    Thread* t = (Thread*) _t;
    free(t);
}

void TxInitThread(void* _t, long id) {
    Thread* t = (Thread*) _t;
    *t = Thread(id);
}

/* =============================================================================
 * TxAlloc
 *
 * CCM: simple transactional memory allocation
 * =============================================================================
 */
void* TxAlloc(void* _Self, size_t size) {
    Thread* Self = (Thread*) _Self;
    void* ptr = tmalloc_reserve(size);
    if (ptr) {
        tmalloc_append(Self->allocPtr, ptr);
    }

    return ptr;
}

/* =============================================================================
 * TxFree
 *
 * CCM: simple transactional memory de-allocation
 * =============================================================================
 */
void TxFree(void* _Self, void* ptr) {
    Thread* Self = (Thread*) _Self;
    tmalloc_append(Self->freePtr, ptr);
}

long TxLoadl(void* _Self, volatile long* addr) {
    Thread* Self = (Thread*) _Self;
    return TxLoad(Self, addr);
}
float TxLoadf(void* _Self, volatile float* addr) {
    Thread* Self = (Thread*) _Self;
    return TxLoad(Self, addr);
}

long TxStorel(void* _Self, volatile long* addr, long value) {
    Thread* Self = (Thread*) _Self;
    return TxStore(Self, addr, value);
}
float TxStoref(void* _Self, volatile float* addr, float value) {
    Thread* Self = (Thread*) _Self;
    return TxStore(Self, addr, value);
}
