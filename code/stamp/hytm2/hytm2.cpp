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

#define TM_NAME "HyTM2"
#define HTM_RETRY_THRESH 5

volatile long StartTally = 0;
volatile long AbortTally = 0;










/**
 * 
 * TRY-LOCK IMPLEMENTATION AND LOCK TABLE
 * 
 */

class Thread;

#define LOCKBIT 1
class vLockSnapshot {
private:
    uintptr_t lockstate;
public:
    vLockSnapshot() {}
    vLockSnapshot(uintptr_t _lockstate) {
        lockstate = _lockstate;
    }
    bool isLocked() {
        return lockstate & LOCKBIT;
    }
    bool version() {
        return lockstate & ~LOCKBIT;
    }
};

class vLock {
    volatile uintptr_t lock; // (Version,LOCKBIT)
private:
    vLock(uintptr_t lockstate) {
        lock = lockstate;
    }
public:
    vLock() {
        lock = 0;
    }
    vLockSnapshot getSnapshot() {
        return vLockSnapshot(lock);
    }
    bool tryAcquire() {
        uintptr_t val = lock & ~LOCKBIT;
        return __sync_bool_compare_and_swap(&lock, val, val+1);
    }
    void release() {
        ++lock;
    }
    // can be invoked only by a hardware transaction
    void htmIncrementVersion() {
        lock += LOCKBIT*2;
    }
};

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
    long Starts;
    long Aborts; /* Tally of # of aborts */
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
    union {
        volatile long* l;
        volatile float* f;
        volatile intptr_t* p;
    } addr;
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
        addr.l = 0;
        value.l = 0;
    }
};

template <typename T>
inline void assignAV(AVPair* e, volatile T* addr, T value);
template <>
inline void assignAV<long>(AVPair* e, volatile long* addr, long value) {
    e->addr.l = addr;
    e->value.l = value;
}
template <>
inline void assignAV<float>(AVPair* e, volatile float* addr, float value) {
    e->addr.f = addr;
    e->value.f[0] = value;
}

template <typename T>
inline void replayStore(AVPair* e);
template <>
inline void replayStore<long>(AVPair* e) {
    *(e->addr.l) = e->value.l;
}
template <>
inline void replayStore<float>(AVPair* e) {
    *(e->addr.f) = e->value.f[0];
}

template <typename T>
inline volatile T* unpackAddr(AVPair* e);
template <>
inline volatile long* unpackAddr<long>(AVPair* e) {
    return e->addr.l;
}
template <>
inline volatile float* unpackAddr<float>(AVPair* e) {
    return e->addr.f;
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
    AVPair* List;
    char padding[CACHE_LINE_SIZE];
    AVPair* put;    /* Insert position - cursor */
    AVPair* tail;   /* CCM: Pointer to last valid entry */
    AVPair* end;    /* CCM: Pointer to last entry */
    long ovf;       /* Overflow - request to grow */
    long sz;

private:
    __INLINE__ AVPair* extend() {
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

public:
    Log() {}
    Log(long _sz, Thread* Self) {
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
    }
    
    void clear() {
        put = List;
        tail = NULL;
    }

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    __INLINE__ void append(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
        AVPair* e = put;
        if (e == NULL) {
            e = extend();
        }
        tail = e;
        put = e->Next;
        assignAV(e, Addr, Valu);
        e->LockFor = _LockFor;
        e->rdv = _rdv;
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
    
    // Search for first log entry that contains lock.
    __INLINE__ AVPair* findFirst(vLock* Lock) {
        AVPair* const End = put;
        for (AVPair* e = List; e != End; e = e->Next) {
            if (e->LockFor == Lock) {
                return e;
            }
        }
        return NULL;
    }
    
    // Search for last log entry that contains lock.
    __INLINE__ AVPair* findLast(vLock* Lock) {
        for (AVPair* e = tail; e != NULL; e = e->Prev) {
            if (e->LockFor == Lock) {
                return e;
            }
        }
        return NULL;
    }
    
    __INLINE__ bool contains(vLock* Lock) {
        return findFirst(Lock) != NULL;
    }
    
    // can be invoked only by a transaction on the software path
    __INLINE__ bool lockAll(Thread* Self) {
        assert(Self->isFallback);
        
        AVPair* const stop = put;
        for (AVPair* curr = List; curr != stop; curr = curr->Next) {
            // if we fail to acquire a lock
            if (!curr->LockFor->tryAcquire()) {
                // unlock all previous entries (since we locked them) and return false
                for (AVPair* toUnlock = List; toUnlock != curr; toUnlock = toUnlock->Next) {
                    toUnlock->LockFor->release();
                }
                return false;
            }
        }
        return true;
    }
    
    // can be invoked only by a transaction on the software path
    __INLINE__ void releaseAll(Thread* Self) {
        assert(Self->isFallback);
        
        AVPair* const stop = put;
        for (AVPair* curr = List; curr != stop; curr = curr->Next) {
            curr->LockFor->release();
        }
    }

    // can be invoked only by a transaction on the software path.
    // writeSet must point to the write-set for this Thread that
    // contains addresses/values of type T.
    template <typename T>
    __INLINE__ bool validate(Thread* Self) {
        assert(Self->isFallback);
        
        AVPair* const stop = put;
        for (AVPair* curr = List; curr != stop; curr = curr->Next) {
            volatile T* Addr = unpackAddr<T>(curr);
            vLock* lock = PSLOCK(Addr);
            vLockSnapshot locksnap = lock->getSnapshot();
            if (locksnap.isLocked() || locksnap.version() != curr->rdv.version()) {
                // if write set does not contain Addr
                Log* wrSet = getTypedLog<T>(Self->wrSet);
                if (!wrSet->contains(lock)) { // TODO: improve contains w/hashing or bloom filter
                    return false;
                }
            }
        }
        return true;
    }
};

class TypeLogs {
public:
    Log l;
    Log f;
    long sz;
    
    TypeLogs() {}
    TypeLogs(Thread* Self, long _sz) {
        sz = _sz;
        l = Log(sz, Self);
        f = Log(sz, Self);
    }
    void destroy() {
        l.destroy();
        f.destroy();
    }

    __INLINE__ void clear() {
        l.clear();
        f.clear();
    }

    __INLINE__ bool lockAll(Thread* Self) {
        return l.lockAll(Self) && f.lockAll(Self);
    }
    
    __INLINE__ void releaseAll(Thread* Self) {
        l.releaseAll(Self);
        f.releaseAll(Self);
    }
    
    __INLINE__ bool validate(Thread* Self) {
        return l.validate<long>(Self) && f.validate<float>(Self);
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
    __INLINE__ void append(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
        Log *_log = getTypedLog<T>(this);
        _log->append(Addr, Valu, _LockFor, _rdv);
    }
    
    template <typename T>
    __INLINE__ AVPair* findFirst(vLock* Lock) {
        Log *_log = getTypedLog<T>(this);
        return _log->findFirst(Lock);
    }
    
    template <typename T>
    __INLINE__ AVPair* findLast(vLock* Lock) {
        Log *_log = getTypedLog<T>(this);
        return _log->findLast(Lock);
    }
    
    template <typename T>
    __INLINE__ bool contains(vLock* Lock) {
        Log *_log = getTypedLog<T>(this);
        return _log->contains(Lock);
    }
};

template <>
inline Log * getTypedLog<long>(TypeLogs * typelogs) {
    return &typelogs->l;
}
template <>
inline Log * getTypedLog<float>(TypeLogs * typelogs) {
    return &typelogs->f;
}

__INLINE__ intptr_t AtomicAdd(volatile intptr_t* addr, intptr_t dx) {
    intptr_t v;
    for (v = *addr; CAS(addr, v, v + dx) != v; v = *addr) {
    }
    return (v + dx);
}

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
    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTally)), Aborts);
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
        // check whether Addr is in the write-set
        vLock* lock = PSLOCK(Addr);
        AVPair* av = Self->wrSet->findLast<T>(lock); // TODO: improve find w/hashing or bloom filter
        if (av != NULL) return unpackValue<T>(av);

        // Addr is NOT in the write-set; abort if it is locked
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // read addr and add it to the read-set
        T val = *Addr;
        Self->rdSet->append(Addr, val, lock, locksnap); // TODO: do we want to append EVERY time we read???

        // validate reads
        if (!Self->rdSet->validate(Self)) TxAbort(Self);
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
        // abort if addr is locked
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // add addr to the write-set
        Self->wrSet->append<T>(addr, valu, lock, locksnap);
        Self->IsRO = false; // txn is not read-only
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
    printf("%s %s:\n  Starts=%li Aborts=%li\n",
            TM_NAME, "system shutdown", StartTally, AbortTally);
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
