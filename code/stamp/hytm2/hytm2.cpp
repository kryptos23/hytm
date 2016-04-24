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
#include "../murmurhash/MurmurHash3.h"
#include <iostream>
using namespace std;

#define TM_NAME "HyTM2"
#ifndef HTM_ATTEMPT_THRESH
    #define HTM_ATTEMPT_THRESH 5
#endif
#define TXNL_MEM_RECLAMATION

//#define DEBUG_PRINT
//#define DEBUG_PRINT_LOCK

#ifdef DEBUG_PRINT
    #define aout(x) { \
        cout<<x<<endl; \
    }
#elif defined(DEBUG_PRINT_LOCK)
    #define aout(x) { \
        acquireLock(&globallock); \
        cout<<x<<endl; \
        releaseLock(&globallock); \
    }
#else
    #define aout(x) 
#endif

// just for debugging
volatile int globallock = 0;

void acquireLock(volatile int *lock) {
    while (1) {
        if (*lock) {
            __asm__ __volatile__("pause;");
            continue;
        }
        if (__sync_bool_compare_and_swap(lock, 0, 1)) {
            return;
        }
    }
}

void releaseLock(volatile int *lock) {
    *lock = 0;
}













/**
 * 
 * TRY-LOCK IMPLEMENTATION AND LOCK TABLE
 * 
 */

class Thread;

#define LOCKBIT 1
class vLockSnapshot {
public:
//private:
    uint64_t lockstate;
public:
    vLockSnapshot() {}
    vLockSnapshot(uint64_t _lockstate) {
        lockstate = _lockstate;
    }
    __INLINE__ bool isLocked() const {
        return lockstate & LOCKBIT;
    }
    __INLINE__ uint64_t version() const {
//        cout<<"LOCKSTATE="<<lockstate<<" ~LOCKBIT="<<(~LOCKBIT)<<" VERSION="<<(lockstate & (~LOCKBIT))<<endl;
        return lockstate & (~LOCKBIT);
    }
    friend std::ostream& operator<<(std::ostream &out, const vLockSnapshot &obj);
};

class vLock {
    volatile uint64_t lock; // (Version,LOCKBIT)
private:
    vLock(uint64_t lockstate) {
        lock = lockstate;
    }
public:
    vLock() {
        lock = 0;
    }
    __INLINE__ vLockSnapshot getSnapshot() const {
        return vLockSnapshot(lock);
    }
    __INLINE__ bool tryAcquire() {
        uint64_t val = lock & (~LOCKBIT);
        return __sync_bool_compare_and_swap(&lock, val, val+1);
    }
    __INLINE__ bool tryAcquire(vLockSnapshot& oldval) {
        return __sync_bool_compare_and_swap(&lock, oldval.version(), oldval.version()+1);
    }
    __INLINE__ void release() {
        ++lock;
    }
    // can be invoked only by a hardware transaction
    __INLINE__ void htmIncrementVersion() {
        lock += 2;
    }
};

std::ostream& operator<<(std::ostream& out, const vLockSnapshot& obj) {
    return out<<"<ver="<<obj.version()
                <<",locked="<<obj.isLocked()<<">"
                ;//<<"> (raw="<<obj.lockstate<<")";
}

std::ostream& operator<<(std::ostream& out, const vLock& obj) {
    return out<<obj.getSnapshot()<<"@"<<(uintptr_t)(long*)&obj;
}

/*
 * Consider 4M alignment for LockTab so we can use large-page support.
 * Alternately, we could mmap() the region with anonymous DZF pages.
 */
#define _TABSZ  (1<< 20)
static vLock LockTab[_TABSZ];

/*
 * With PS the versioned lock words (the LockTab array) are table stable and
 * references will never fault.  Under PO, however, fetches by a doomed
 * zombie txn can fault if the referent is free()ed and unmapped
 */
#if 0
#define LDLOCK(a)                     LDNF(a)  /* for PO */
#else
#define LDLOCK(a)                     *(a)     /* for PS */
#endif

/*
 * PSLOCK: maps variable address to lock address.
 * For PW the mapping is simply (UNS(addr)+sizeof(int))
 * COLOR attempts to place the lock(metadata) and the data on
 * different D$ indexes.
 */
#define TABMSK (_TABSZ-1)
#define COLOR (128)

/*
 * ILP32 vs LP64.  PSSHIFT == Log2(sizeof(intptr_t)).
 */
#define PSSHIFT ((sizeof(void*) == 4) ? 2 : 3)
#define PSLOCK(a) (LockTab + (((UNS(a)+COLOR) >> PSSHIFT) & TABMSK)) /* PS1M */










/**
 * 
 * THREAD CLASS
 *
 */

class TypeLogs;

class Thread {
public:
    long UniqID;
    volatile long Retries;
//    int* ROFlag; // not used by stamp
    bool IsRO;
    int isFallback;
//    long Starts; // how many times the user called TxBegin
    long AbortsHW; // # of times hw txns aborted
    long AbortsSW; // # of times sw txns aborted
    long CommitsHW;
    long CommitsSW;
    unsigned long long rng;
    unsigned long long xorrng [1];
    tmalloc_t* allocPtr;    /* CCM: speculatively allocated */
    tmalloc_t* freePtr;     /* CCM: speculatively free'd */
    TypeLogs* rdSet;
    TypeLogs* wrSet;
    TypeLogs* LocalUndo;
    sigjmp_buf* envPtr;
    
    Thread(long id);
    void destroy();
} __attribute__((aligned(CACHE_LINE_SIZE)));











/**
 * 
 * LOG IMPLEMENTATION
 * 
 */

/* Read set and write-set log entry */
class AVPair {
public:
    AVPair* Next;
    AVPair* Prev;
    intptr_t addr;
    union {
        long l;
#ifdef __LP64__
        float f[2];
#else
        float f[1];
#endif
        intptr_t p;
    } value;
    vLock* LockFor;     /* points to the vLock covering Addr */
    vLockSnapshot rdv;  /* read-version @ time of 1st read - observed */
    Thread* Owner;
    long Ordinal;
    
    AVPair() {}
    AVPair(AVPair* _Next, AVPair* _Prev, Thread* _Owner, long _Ordinal)
        : Next(_Next), Prev(_Prev), LockFor(0), rdv(0), Owner(_Owner), Ordinal(_Ordinal) {
        addr = 0;
        value.l = 0;
    }
};

template <typename T>
inline void assignAV(AVPair* e, volatile intptr_t addr, T value);
template <>
inline void assignAV<long>(AVPair* e, volatile intptr_t addr, long value) {
    e->addr = addr;
    e->value.l = value;
}
template <>
inline void assignAV<float>(AVPair* e, volatile intptr_t addr, float value) {
    e->addr = addr;
    e->value.f[0] = value;
}

template <typename T>
inline void replayStore(AVPair* e);
template <>
inline void replayStore<long>(AVPair* e) {
    *((long*)e->addr) = e->value.l;
}
template <>
inline void replayStore<float>(AVPair* e) {
    *((float*)e->addr) = e->value.f[0];
}

template <typename T>
inline T unpackValue(AVPair* e);
template <>
inline long unpackValue<long>(AVPair* e) {
    return e->value.l;
}
template <>
inline float unpackValue<float>(AVPair* e) {
    return e->value.f[0];
}

class Log;

template <typename T>
inline Log * getTypedLog(TypeLogs * typelogs);

enum hytm_config {
    INIT_WRSET_NUM_ENTRY = 1024,
    INIT_RDSET_NUM_ENTRY = 8192,
    INIT_LOCAL_NUM_ENTRY = 1024,
};

class Log {
public:
    // linked list (for iteration)
    AVPair* List;
    char padding[CACHE_LINE_SIZE];
    AVPair* put;    /* Insert position - cursor */
    AVPair* tail;   /* CCM: Pointer to last valid entry */
    AVPair* end;    /* CCM: Pointer to last entry */
    long ovf;       /* Overflow - request to grow */
    long sz;
    
    // hash table (for fast lookups)
    uint32_t seed;  // seed for hash function
    AVPair** htab;
    long htabsz;    // number of elements in the hash table
    long htabcap;   // capacity of the hash table
    
private:
    __INLINE__ int hash(uintptr_t p) {
        // assert: htabcap is a power of 2
        return (int) MurmurHash3_x86_32_uintptr(p, seed) & (htabcap-1);
    }
    
    __INLINE__ AVPair* extendList() {
        // Append at the tail. We want the front of the list,
        // which sees the most traffic, to remains contiguous.
        ovf++;
        AVPair* e = (AVPair*) malloc(sizeof(*e));
        assert(e);
        tail->Next = e;
        *e = AVPair(NULL, tail, tail->Owner, tail->Ordinal+1);
        end = e;
        return e;
    }
    
    // TODO: fix error: if two addresses are protected by the same lock, then only one will be added to the log!
    __INLINE__ int htabFindIx(uintptr_t addr) {
        int ix = hash(addr);
        while (htab[ix]) {
            if (htab[ix]->addr == addr) {
                return ix;
            }
            ix = (ix + 1) & (htabcap-1);
        }
        return -1;
    }
    
    __INLINE__ AVPair* htabFind(uintptr_t addr) {
        int ix = htabFindIx(addr);
        if (ix < 0) return NULL;
        return htab[ix];
    }
    
    // assumes there is space for e
    __INLINE__ void htabInsert(uintptr_t addr, AVPair* e) {
        int ix = hash(addr);
        while (htab[ix]) { // assumes hash table does NOT contain e
            ix = (ix + 1) & (htabcap-1);
        }
        htab[ix] = e;
    }
    
    __INLINE__ void extendHashTable() {
        // expand table by a factor of 2
        AVPair** old = htab;
        htabcap *= 2;
        htab = (AVPair**) malloc(sizeof(AVPair*) * htabcap);
        memset(htab, 0, sizeof(AVPair*) * htabcap);
        free(old);
        
        // rehash elements into new table
        AVPair* End = put;
        for (AVPair* e = List; e != End; e = e->Next) {
            htabInsert(e->addr, e);
        }
    }

public:
    Log() {}
    Log(long _sz, Thread* Self) {
        // assert: _sz is a power of 2
        
        // Allocate the primary list as a large chunk so we can guarantee ascending &
        // adjacent addresses through the list. This improves D$ and DTLB behavior.
        List = (AVPair*) malloc((sizeof(AVPair) * _sz) + CACHE_LINE_SIZE);
        assert(List);
        memset(List, 0, sizeof(AVPair) * _sz);
        long i;
        AVPair* curr = List;
        put = List;
        end = NULL;
        tail = NULL;
        for (i = 0; i < _sz; i++) {
            AVPair* e = curr++;
            *e = AVPair(curr, tail, Self, i); // note: curr is invalid in the last iteration
            tail = e;
        }
        tail->Next = NULL; // fix invalid next pointer from last iteration
        sz = _sz;
        ovf = 0;
        
        // allocate memory for the hash table
        seed = 0xc4d1ca82; // for hash function (drawn from atmospheric noise)
        htab = (AVPair**) malloc(sizeof(AVPair*) * _sz);
        htabsz = 0;
        htabcap = _sz;
        memset(htab, 0, sizeof(AVPair*) * _sz);
    }

    void destroy() {
        /* Free appended overflow entries first */
        AVPair* e = end;
        if (e != NULL) {
            while (e->Ordinal >= sz) {
                AVPair* tmp = e;
                e = e->Prev;
                free(tmp);
            }
        }
        /* Free contiguous beginning */
        free(List);
        
        // free hash table
        free(htab);
    }
    
    void clear() {
        // clear hash table by nulling out slots for the AVPairs in the list
        AVPair* End = put;
        for (AVPair* e = List; e != End; e = e->Next) {
            int ix = htabFindIx(e->addr);
            if (ix < 0) continue;
            htab[ix] = NULL;
        }
        
        // clear list
        put = List;
        tail = NULL;
    }

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    __INLINE__ void insert(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
        AVPair* e = htabFind((uintptr_t) Addr);
        
        // this address is NOT in the log
        if (e == NULL) {
            // add it to the list
            AVPair* e = put;
            if (e == NULL) e = extendList();
            tail = e;
            put = e->Next;
            assignAV(e, (uintptr_t) Addr, Valu);
            e->LockFor = _LockFor;
            e->rdv = _rdv;
            
            // add it to the hash table
            // (first, expand the table if it is more than half full)
            if (2*htabsz > htabcap) extendHashTable();
            htabInsert((uintptr_t) Addr, e);
            
        // this address is already in the log
        } else {
            // update the value associated with Addr
            assignAV(e, (uintptr_t) Addr, Valu);
            // note: we keep the old position in the list,
            // and the old lock version number (and everything else).
        }
    }
    
    // Transfer the data in the log to its ultimate location.
    template <typename T>
    __INLINE__ void writeReverse() {
        for (AVPair* e = tail; e != NULL; e = e->Prev) {
            replayStore<T>(e);
        }
    }

    // Transfer the data in the log to its ultimate location.
    template <typename T>
    __INLINE__ void writeForward() {
        AVPair* End = put;
        for (AVPair* e = List; e != End; e = e->Next) {
            replayStore<T>(e);
        }
    }
    
    __INLINE__ AVPair* find(uintptr_t addr) {
        return htabFind(addr);
    }
    
    __INLINE__ bool contains(uintptr_t addr) {
        return htabFindIx(addr) != -1;
    }
};

class TypeLogs {
public:
    Log l;
    Log f;
    long sz;
    
    TypeLogs() {}
    TypeLogs(Thread* Self, long _sz)
    : sz(_sz), l(Log(_sz, Self)), f(Log(_sz, Self)) {}
    
    void destroy() {
        l.destroy();
        f.destroy();
    }

    __INLINE__ void clear() {
        l.clear();
        f.clear();
    }
    
    __INLINE__ void writeForward() {
        l.writeForward<long>();
        f.writeForward<float>();
    }

    __INLINE__ void writeReverse() {
        l.writeReverse<long>();
        f.writeReverse<float>();
    }
    
    template <typename T>
    __INLINE__ void insert(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
        Log *_log = getTypedLog<T>(this);
        _log->insert(Addr, Valu, _LockFor, _rdv);
    }
    
    template <typename T>
    __INLINE__ AVPair* find(uintptr_t addr) {
        Log *_log = getTypedLog<T>(this);
        return _log->find(addr);
    }
    
    template <typename T>
    __INLINE__ bool contains(uintptr_t addr) {
        Log *_log = getTypedLog<T>(this);
        return _log->contains(addr);
    }
//    
//    template <typename T>
//    __INLINE__ vLockSnapshot* getLockSnapshot(AVPair* e) {
//        AVPair* av = find<T>(e->LockFor);
//        if (av) return &av->rdv;
//        return NULL;
//    }
};

template <>
__INLINE__ Log * getTypedLog<long>(TypeLogs * typelogs) {
    return &typelogs->l;
}
template <>
__INLINE__ Log * getTypedLog<float>(TypeLogs * typelogs) {
    return &typelogs->f;
}

__INLINE__ vLockSnapshot* minLockSnapshot(vLockSnapshot* a, vLockSnapshot* b) {
    if (a && b) {
        return (a->version() < b->version()) ? a : b;
    } else {
        return a ? a : b; // note: might return NULL
    }
}

__INLINE__ vLockSnapshot* getMinLockSnap(AVPair* a, AVPair* b) {
    if (a && b) {
        return minLockSnapshot(&a->rdv, &b->rdv);
    } else {
        return a ? &a->rdv : (b ? &b->rdv : NULL);
    }
}

// can be invoked only by a transaction on the software path
template <typename T>
__INLINE__ bool lockAll(Thread* Self, Log* log) {
    assert(Self->isFallback);

    AVPair* const stop = log->put;
    for (AVPair* curr = log->List; curr != stop; curr = curr->Next) {
        // determine when we first encountered curr->addr in txload or txstore.
        // curr->rdv contains when we first encountered it in a txSTORE,
        // so we also need to compare it with any rdv stored in the READ-set.
        AVPair* readLogEntry = Self->rdSet->find<T>(curr->addr);
        vLockSnapshot *encounterTime = getMinLockSnap(curr, readLogEntry);
        assert(encounterTime);
//        if (encounterTime->version() != readLogEntry->rdv.version()) {
//            fprintf(stderr, "WARNING: read encounter time was not taken as the minimum\n");
//        }
        
//        vLockSnapshot currsnap = curr->rdv;
        aout("thread "<<Self->UniqID<<" trying to acquire lock "
                    <<*curr->LockFor
                    <<" with old-ver "<<encounterTime->version()
                    //<<" (and curr-ver "<<currsnap.version()<<" raw="<<currsnap.lockstate<<" raw&(~1)="<<(currsnap.lockstate&(~1))<<")"
                    <<" to protect val "<<(*((T*) curr->addr))
                    <<" (and write new-val "<<unpackValue<T>(curr)<<")");

        // try to acquire locks
        // (and fail if their versions have changed since encounterTime)
        if (!curr->LockFor->tryAcquire(*encounterTime)) {
            // if we fail to acquire a lock, we must
            // unlock all previous locks that we acquired
            for (AVPair* toUnlock = log->List; toUnlock != curr; toUnlock = toUnlock->Next) {
                toUnlock->LockFor->release();
//                aout("thread "<<Self->UniqID<<" releasing lock "<<*curr->LockFor<<" in lockAll (will be ver "<<(curr->LockFor->getSnapshot().version()+2)<<")");
            }
//            aout("thread "<<Self->UniqID<<" failed to acquire lock "<<*curr->LockFor);
            return false;
        }
//        aout("thread "<<Self->UniqID<<" acquired lock "<<*curr->LockFor);
    }
    return true;
}

__INLINE__ bool tryLockWriteSet(Thread* Self) {
    return lockAll<long>(Self, &Self->wrSet->l)
        && lockAll<float>(Self, &Self->wrSet->f);
}

// NOT TO BE INVOKED DIRECTLY
// releases all locks on addresses in a Log
__INLINE__ void releaseAll(Thread* Self, Log* log) {
    AVPair* const stop = log->put;
    for (AVPair* curr = log->List; curr != stop; curr = curr->Next) {
        aout("thread "<<Self->UniqID<<" releasing lock "<<*curr->LockFor<<" in releaseAll (will be ver "<<(curr->LockFor->getSnapshot().version()+2)<<")");
        curr->LockFor->release();
    }
}

// can be invoked only by a transaction on the software path
// releases all locks on addresses in the write-set
__INLINE__ void releaseWriteSet(Thread* Self) {
    assert(Self->isFallback);
    releaseAll(Self, &Self->wrSet->l);
    releaseAll(Self, &Self->wrSet->f);
}

// can be invoked only by a transaction on the software path.
// writeSet must point to the write-set for this Thread that
// contains addresses/values of type T.
template <typename T>
__INLINE__ bool validate(Thread* Self, Log* log, bool holdingLocks) {
    assert(Self->isFallback);

    AVPair* const stop = log->put;
    for (AVPair* curr = log->List; curr != stop; curr = curr->Next) {
        // determine when we first encountered curr->addr in txload or txstore.
        // curr->rdv contains when we first encountered it in a txLOAD,
        // so we also need to compare it with any rdv stored in the WRITE-set.
        AVPair* writeLogEntry = Self->wrSet->find<T>(curr->addr);
        vLockSnapshot *encounterTime = getMinLockSnap(curr, writeLogEntry);
        assert(encounterTime);
        
        vLock* lock = curr->LockFor;
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked() || locksnap.version() != encounterTime->version()) {
            // the address is locked, it its version number has changed
            // we abort if we are not the one who holds a lock on it, or changed its version.
            if (!holdingLocks) {
                return false; // abort if we are not holding any locks
            } else if (!getTypedLog<T>(Self->wrSet)->contains(curr->addr)) {
                return false; // abort if we are holding locks, but addr is not in our write-set
            }
//            return false;
        }
    }
    return true;
}

__INLINE__ bool validateReadSet(Thread* Self, bool holdingLocks) {
    return validate<long>(Self, &Self->rdSet->l, holdingLocks)
        && validate<float>(Self, &Self->rdSet->f, holdingLocks);
}

__INLINE__ intptr_t AtomicAdd(volatile intptr_t* addr, intptr_t dx) {
    intptr_t v;
    for (v = *addr; CAS(addr, v, v + dx) != v; v = *addr) {}
    return (v + dx);
}










/**
 * 
 * THREAD CLASS IMPLEMENTATION
 * 
 */

volatile long StartTally = 0;
volatile long AbortTallyHW = 0;
volatile long AbortTallySW = 0;
volatile long CommitTallyHW = 0;
volatile long CommitTallySW = 0;

Thread::Thread(long id) {
    memset(this, 0, sizeof (*this)); /* Default value for most members */
    UniqID = id;
    rng = id + 1;
    xorrng[0] = rng;

    wrSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    rdSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    LocalUndo = (TypeLogs*) malloc(sizeof(TypeLogs));
    *wrSet = TypeLogs(this, INIT_WRSET_NUM_ENTRY);
    *rdSet = TypeLogs(this, INIT_RDSET_NUM_ENTRY);
    *LocalUndo = TypeLogs(this, INIT_LOCAL_NUM_ENTRY);

    allocPtr = tmalloc_alloc(1);
    freePtr = tmalloc_alloc(1);
    assert(allocPtr);
    assert(freePtr);
}

void Thread::destroy() {
//    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTallySW)), AbortsSW);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTallyHW)), AbortsHW);
    AtomicAdd((volatile intptr_t*)((void*) (&CommitTallySW)), CommitsSW);
    AtomicAdd((volatile intptr_t*)((void*) (&CommitTallyHW)), CommitsHW);
    tmalloc_free(allocPtr);
    tmalloc_free(freePtr);
    wrSet->destroy();
    rdSet->destroy();
    LocalUndo->destroy();
    free(wrSet);
    free(rdSet);
    free(LocalUndo);
}










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

    unsigned status;
    if (aborted_in_software) goto software;

//    Self->Starts++;
    Self->Retries = 0;
    Self->isFallback = 0;
//    Self->ROFlag = ROFlag;
    Self->IsRO = true;
    Self->envPtr = envPtr;
    if (HTM_ATTEMPT_THRESH <= 0) goto software;
htmretry:
    status = XBEGIN();
    if (status != _XBEGIN_STARTED) { // if we aborted
        ++Self->AbortsHW;
        if (++Self->Retries < HTM_ATTEMPT_THRESH) goto htmretry;
        else goto software;
    }
    return;
software:
//    aout("thread "<<Self->UniqID<<" started tx attempt "<<Self->Starts);
    Self->isFallback = 1;
}

int TxCommit(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        // return immediately if txn is read-only
        if (Self->IsRO) goto success;
        
        // lock all addresses in the write-set
        if (!tryLockWriteSet(Self)) {
            TxAbort(Self); // abort if we fail to acquire some lock
            // (note: after lockAll we hold either all or no locks)
        }
        
        // validate reads
        if (!validateReadSet(Self, true)) { // TODO: needs to be since first time i READ OR WROTE
            // if we fail validation, release all locks and abort
            aout("thread "<<Self->UniqID<<" validation failed -> releasing write-set");
            releaseWriteSet(Self);
            TxAbort(Self);
        }
        
        // perform the actual writes
        Self->wrSet->writeForward();
        
        // release all locks
        aout("thread "<<Self->UniqID<<" committed -> releasing write-set");
        releaseWriteSet(Self);
        ++Self->CommitsSW;
        
    // hardware path
    } else {
        XEND();
        ++Self->CommitsHW;
    }
    
success:
#ifdef TXNL_MEM_RECLAMATION
    // "commit" speculative frees and speculative allocations
    tmalloc_releaseAllForward(Self->freePtr, NULL);
    tmalloc_clear(Self->allocPtr);
#endif
    return true;
}

void TxAbort(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        ++Self->Retries;
        ++Self->AbortsSW;
//        if (Self->Aborts > 1000) { fprintf(stderr, "TOO MANY ABORTS. QUITTING.\n"); exit(-1); }
        
#ifdef TXNL_MEM_RECLAMATION
        // "abort" speculative allocations and speculative frees
        tmalloc_releaseAllReverse(Self->allocPtr, NULL);
        tmalloc_clear(Self->freePtr);
#endif
        
        // longjmp to start of txn
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
//        printf("txLoad(id=%ld, addr=0x%lX) on fallback\n", Self->UniqID, (unsigned long)(void*) Addr);
        
        // check whether Addr is in the write-set
        AVPair* av = Self->wrSet->find<T>((uintptr_t) Addr);
        if (av != NULL) return unpackValue<T>(av);

        // Addr is NOT in the write-set; abort if it is locked
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // read addr and add it to the read-set
        T val = *Addr;
        Self->rdSet->insert(Addr, val, lock, locksnap);

        // validate reads
        if (!validateReadSet(Self, false)) TxAbort(Self);

//        printf("    txLoad(id=%ld, ...) success\n", Self->UniqID);
        return val;
        
    // hardware path
    } else {
        // abort if addr is locked
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // actually read addr
        return *Addr;
    }
}

template <typename T>
T TxStore(void* _Self, volatile T* addr, T valu) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
//        printf("txStore(id=%ld, addr=0x%lX, val=%ld) on fallback\n", Self->UniqID, (unsigned long)(void*) addr, (long) valu);
        
        // abort if addr is locked (since we do not hold any locks)
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // add addr to the write-set
        Self->wrSet->insert<T>(addr, valu, lock, locksnap);
        Self->IsRO = false; // txn is not read-only

//        printf("    txStore(id=%ld, ...) success\n", Self->UniqID);
        return valu;
        
    // hardware path
    } else {
        // abort if addr is locked
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) XABORT(_XABORT_EXPLICIT_LOCKED);
        
        // increment version number (to notify s/w txns of the change)
        // and write value to addr (order unimportant because of h/w)
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
    memset(LockTab, 0, _TABSZ*sizeof(vLock));
}

void TxShutdown() {
    printf("%s system shutdown:\n  Starts=%li CommitsHW=%li AbortsHW=%li CommitsSW=%li AbortsSW=%li\n",
                TM_NAME,
                CommitTallyHW+CommitTallySW,
                CommitTallyHW, AbortTallyHW,
                CommitTallySW, AbortTallySW);
}

void* TxNewThread() {
    Thread* t = (Thread*) malloc(sizeof(Thread));
    assert(t);
    return t;
}

void TxFreeThread(void* _t) {
    Thread* t = (Thread*) _t;
    t->destroy();
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
#ifdef TXNL_MEM_RECLAMATION
    Thread* Self = (Thread*) _Self;
    void* ptr = tmalloc_reserve(size);
    if (ptr) {
        tmalloc_append(Self->allocPtr, ptr);
    }

    return ptr;
#else
    return malloc(size);
#endif
}

/* =============================================================================
 * TxFree
 *
 * CCM: simple transactional memory de-allocation
 * =============================================================================
 */
void TxFree(void* _Self, void* ptr) {
#ifdef TXNL_MEM_RECLAMATION
    Thread* Self = (Thread*) _Self;
    tmalloc_append(Self->freePtr, ptr);
#else
    free(ptr);
#endif
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
