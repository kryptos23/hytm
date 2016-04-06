/**
 * Code for HyTM is loosely based on the code for TL2
 * (in particular, the data structures)
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
#include "hytm1.h"
#include "util.h"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE           64
#endif

enum hytm1_config {
    INIT_WRSET_NUM_ENTRY = 1024,
    INIT_RDSET_NUM_ENTRY = 8192,
    INIT_LOCAL_NUM_ENTRY = 1024,
};

#define HTM_RETRY_THRESH          5

typedef uintptr_t vwLock; /* (Version,LOCKBIT) */

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

/* Read set and write-set log entry */
typedef struct _AVPair {
    struct _AVPair* Next;
    struct _AVPair* Prev;
    union {
        volatile long* l;
        volatile float* f;
        volatile intptr_t* p;
    } addr;
    //volatile intptr_t* Addr;
    //intptr_t Valu;
    union {
        long l;
#ifdef __LP64__
        float f[2];
#else
        float f[1];
#endif
        intptr_t p;
    } value;
    volatile vwLock* LockFor; /* points to the vwLock covering Addr */
    vwLock rdv; /* read-version @ time of 1st read - observed */
    struct _Thread* Owner;
    long Ordinal;
} AVPair;

typedef struct _Log {
    AVPair* List;
    AVPair* put; /* Insert position - cursor */
    AVPair* tail; /* CCM: Pointer to last valid entry */
    AVPair* end; /* CCM: Pointer to last entry */
    long ovf; /* Overflow - request to grow */
} Log;

typedef struct _TypeLogs {
    Log l;
    Log f;
    Log p;
} TypeLogs;

struct _Thread {
    long UniqID;
    volatile long Retries;
    int* ROFlag;
//    int IsRO; // skip so that first 8 variables fit in a single cache line
    int isFallback;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
//    tmalloc_t* allocPtr; /* CCM: speculatively allocated */
//    tmalloc_t* freePtr; /* CCM: speculatively free'd */
    TypeLogs rdSet;
    TypeLogs wrSet;
    TypeLogs LocalUndo;
    sigjmp_buf* envPtr;
}; __attribute__((aligned(CACHE_LINE_SIZE)))

__INLINE__ unsigned long long MarsagliaXORV(unsigned long long x) {
    if (x == 0) {
        x = 1;
    }
    x ^= x << 6;
    x ^= x >> 21;
    x ^= x << 7;
    return x;
}

__INLINE__ unsigned long long MarsagliaXOR(unsigned long long* seed) {
    unsigned long long x = MarsagliaXORV(*seed);
    *seed = x;
    return x;
}

__INLINE__ intptr_t AtomicAdd(volatile intptr_t* addr, intptr_t dx) {
    intptr_t v;
    for (v = *addr; CAS(addr, v, v + dx) != v; v = *addr) {
    }
    return (v + dx);
}


/*
 * Consider 4M alignment for LockTab so we can use large-page support.
 * Alternately, we could mmap() the region with anonymous DZF pages.
 */
#define _TABSZ  (1<< 20)
static volatile vwLock LockTab[_TABSZ];

volatile long StartTally = 0;
volatile long AbortTally = 0;
//volatile long ReadOverflowTally  = 0;
//volatile long WriteOverflowTally = 0;
//volatile long LocalOverflowTally = 0;

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
#define TABMSK                        (_TABSZ-1)
#define COLOR                           (128)

/*
 * ILP32 vs LP64.  PSSHIFT == Log2(sizeof(intptr_t)).
 */
#define PSSHIFT                         ((sizeof(void*) == 4) ? 2 : 3)
#define PSLOCK(a) (LockTab + (((UNS(a)+COLOR) >> PSSHIFT) & TABMSK)) /* PS1M */

/* =============================================================================
 * MakeList
 *
 * Allocate the primary list as a large chunk so we can guarantee ascending &
 * adjacent addresses through the list. This improves D$ and DTLB behavior.
 * =============================================================================
 */
__INLINE__ AVPair* MakeList(long sz, Thread* Self) {
    AVPair* ap = (AVPair*) malloc((sizeof (*ap) * sz) + CACHE_LINE_SIZE);
    assert(ap);
    memset(ap, 0, sizeof (*ap) * sz);
    AVPair* List = ap;
    AVPair* Tail = NULL;
    long i;
    for (i = 0; i < sz; i++) {
        AVPair* e = ap++;
        e->Next = ap;
        e->Prev = Tail;
        e->Owner = Self;
        e->Ordinal = i;
        Tail = e;
    }
    Tail->Next = NULL;

    return List;
}

__INLINE__ void MakeTypeLogs(Thread* Self, TypeLogs* logs, long sz) {
    logs->l.List = MakeList(sz, Self);
    logs->l.put = logs->l.List;
    logs->f.List = MakeList(sz, Self);
    logs->f.put = logs->f.List;
    logs->p.List = MakeList(sz, Self);
    logs->p.put = logs->p.List;
}

__INLINE__ void ClearTypeLogs(TypeLogs* logs) {
    logs->l.put = logs->l.List;
    logs->l.tail = NULL;
    logs->f.put = logs->f.List;
    logs->f.tail = NULL;
    logs->p.put = logs->p.List;
    logs->p.tail = NULL;
}

void FreeList(Log*, long) __attribute__((noinline));

/*__INLINE__*/ void FreeList(Log* k, long sz) {
    /* Free appended overflow entries first */
    AVPair* e = k->end;
    if (e != NULL) {
        while (e->Ordinal >= sz) {
            AVPair* tmp = e;
            e = e->Prev;
            free(tmp);
        }
    }
    /* Free continguous beginning */
    free(k->List);
}
               
void FreeTypeLogs(TypeLogs* logs, long sz) {
    FreeList(&logs->l, sz);
    FreeList(&logs->f, sz);
    FreeList(&logs->p, sz);
}

/* =============================================================================
 * ExtendList
 *
 * Postpend at the tail. We want the front of the list, which sees the most
 * traffic, to remains contiguous.
 * =============================================================================
 */
__INLINE__ AVPair* ExtendList(AVPair* tail) {
    AVPair* e = (AVPair*) malloc(sizeof (*e));
    assert(e);
    memset(e, 0, sizeof (*e));
    tail->Next = e;
    e->Prev = tail;
    e->Next = NULL;
    e->Owner = tail->Owner;
    e->Ordinal = tail->Ordinal + 1;
    /*e->Held    = 0; -- done by memset*/
    return e;
}

/* =============================================================================
 * WriteBackForward
 *
 * Transfer the data in the log to its ultimate location.
 * =============================================================================
 */

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
//template <>
//inline void replayStore<intptr_t>(AVPair* e) {
//    *(e->addr.p) = e->value.p;
//}

template <typename T>
__INLINE__ void SingleTypeLogWriteBackForward(Log* k) {
    AVPair* e;
    AVPair* End = k->put;
    for (e = k->List; e != End; e = e->Next) {
        replayStore<T>(e);
        //*(e->Addr) = e->Valu;
    }
}

__INLINE__ void WriteBackForward(TypeLogs* logs) {
    SingleTypeLogWriteBackForward<long>(&logs->l);
    SingleTypeLogWriteBackForward<float>(&logs->f);
    SingleTypeLogWriteBackForward<intptr_t>(&logs->p);
}

/* =============================================================================
 * WriteBackReverse
 *
 * Transfer the data in the log to its ultimate location.
 * =============================================================================
 */
template <typename T>
__INLINE__ void SingleTypeLogWriteBackReverse(Log* k) {
    AVPair* e;
    for (e = k->tail; e != NULL; e = e->Prev) {
        replayStore<T>(e);
        //*(e->Addr) = e->Valu;
    }
}

__INLINE__ void WriteBackReverse(TypeLogs* logs) {
    SingleTypeLogWriteBackReverse<long>(&logs->l);
    SingleTypeLogWriteBackReverse<float>(&logs->f);
    SingleTypeLogWriteBackReverse<intptr_t>(&logs->p);
}

///* =============================================================================
// * FindFirst
// *
// * Search for first log entry that contains lock.
// * =============================================================================
// */
//__INLINE__ AVPair* FindFirst(Log* k, volatile vwLock* Lock) {
//    AVPair* e;
//    AVPair * const End = k->put;
//    for (e = k->List; e != End; e = e->Next) {
//        if (e->LockFor == Lock) {
//            return e;
//        }
//    }
//    return NULL;
//}

//// for redo log: deferred writes -> write-back phase
//__INLINE__ void RecordStore(Log* k, volatile intptr_t* Addr, intptr_t Valu, volatile vwLock* Lock) {
//    /*
//     * As an optimization we could squash multiple stores to the same location.
//     * Maintain FIFO order to avoid WAW hazards.
//     * TODO-FIXME - CONSIDER
//     * Keep Self->LockSet as a sorted linked list of unique LockFor addresses.
//     * We'd scan the LockSet for Lock.  If not found we'd insert a new
//     * LockRecord at the appropriate location in the list.
//     * Call InsertIfAbsent (Self, LockFor)
//     */
//    AVPair* e = k->put;
//    if (e == NULL) {
//        k->ovf++;
//        e = ExtendList(k->tail);
//        k->end = e;
//    }
//    ASSERT(Addr != NULL);
//    k->tail = e;
//    k->put = e->Next;
//    e->Addr = Addr;
//    e->Valu = Valu;
//    e->LockFor = Lock;
//    //    e->Held    = 0;
//    e->rdv = 0; //LOCKBIT; /* use either 0 or LOCKBIT */
//}

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
//template <>
//inline void assignAV<intptr_t>(AVPair* e, volatile intptr_t* addr, intptr_t value) {
//    e->addr.p = addr;
//    e->value.p = value;
//}

// for undo log: immediate writes -> undo on abort/restart
template <typename T>
__INLINE__ void SaveForRollBack(Log* k, volatile T* Addr, T Valu) {
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    k->tail = e;
    k->put = e->Next;
    assignAV(e, Addr, Valu);
    e->LockFor = NULL;
}

void TxOnce() {
    CTASSERT((_TABSZ & (_TABSZ - 1)) == 0); /* must be power of 2 */
    printf("HyTM1 system ready\n");
}

void TxShutdown() {
    printf("HyTM1 system shutdown:\n"
            "  Starts=%li Aborts=%li\n",
            StartTally, AbortTally);
}

Thread* TxNewThread() {
    Thread* t = (Thread*) malloc(sizeof (Thread));
    assert(t);
    return t;
}

void TxFreeThread(Thread* t) {
    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), t->Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTally)), t->Aborts);
    FreeTypeLogs(&t->wrSet, INIT_WRSET_NUM_ENTRY);
    FreeTypeLogs(&t->rdSet, INIT_RDSET_NUM_ENTRY);
    FreeTypeLogs(&t->LocalUndo, INIT_LOCAL_NUM_ENTRY);
    free(t);
}

void TxInitThread(Thread* t, long id) {
    memset(t, 0, sizeof (*t)); /* Default value for most members */

    t->UniqID = id;
    t->rng = id + 1;
    t->xorrng[0] = t->rng;

    MakeTypeLogs(t, &t->wrSet, INIT_WRSET_NUM_ENTRY);
    MakeTypeLogs(t, &t->rdSet, INIT_RDSET_NUM_ENTRY);
    MakeTypeLogs(t, &t->LocalUndo, INIT_LOCAL_NUM_ENTRY);
}

template <typename T>
inline Log * getTypedLog(TypeLogs * typelogs); 
template <>
inline Log * getTypedLog<long>(TypeLogs * typelogs) {
    return &typelogs->l;
}
template <>
inline Log * getTypedLog<float>(TypeLogs * typelogs) {
    return &typelogs->f;
}
//template <>
//inline Log * getTypedLog<intptr_t>(TypeLogs * typelogs) {
//    return &typelogs->p;
//}

template <typename T>
T TxLoad(Thread* Self, volatile T* Addr) {
    return *Addr;
}

template <typename T>
T TxStore(Thread* Self, volatile T* addr, T valu) {
    if (Self->isFallback) {
        SaveForRollBack(getTypedLog<T>(&Self->LocalUndo), addr, *addr);
    }
    return *addr = valu;
}

long TxLoadl(Thread* Self, volatile long* addr) {
    return TxLoad(Self, addr);
}
//intptr_t TxLoadp(Thread* Self, volatile intptr_t* addr) {
//    return TxLoad(Self, addr);
//}
float TxLoadf(Thread* Self, volatile float* addr) {
    return TxLoad(Self, addr);
}

long TxStorel(Thread* Self, volatile long* addr, long value) {
    return TxStore(Self, addr, value);
}
//intptr_t TxStorep(Thread* Self, volatile intptr_t* addr, intptr_t value) {
//    return TxStore(Self, addr, value);
//}
float TxStoref(Thread* Self, volatile float* addr, float value) {
    return TxStore(Self, addr, value);
}


//void TxStoreLocal(Thread* Self, volatile intptr_t* Addr, intptr_t Valu) {
//    if (Self->isFallback) {
//        SaveForRollBack(&Self->LocalUndo, Addr, *Addr);
//    }
//    *Addr = Valu;
//}

void TxStart(Thread* Self, sigjmp_buf* envPtr, int aborted_in_software, int* ROFlag) {
    ClearTypeLogs(&Self->wrSet);
    ClearTypeLogs(&Self->rdSet);
    ClearTypeLogs(&Self->LocalUndo);

    Self->Retries = 0;
    Self->isFallback = 0;
    Self->ROFlag = ROFlag;
    Self->envPtr = envPtr;
    
    int status = -1;
    if (aborted_in_software) {
        goto software;
    }
    Self->Starts++;
htmretry: (0);
    status = XBEGIN();
    if (status == _XBEGIN_STARTED) {
        if (globallock) XABORT(1);
    } else {
        // if we aborted
        ++Self->Aborts;
        if (++Self->Retries < HTM_RETRY_THRESH) {
            // wait until lock is no longer held (to avoid lemmings)
            while (globallock) {
                __asm__ __volatile__("pause;");
            }
            goto htmretry;
        } else {
            goto software;
        }
    }
    return;
software:
    Self->isFallback = 1;
    acquireLock(&globallock);
}

int TxCommit(Thread* Self) {
    if (Self->isFallback) {
        releaseLock(&globallock);
//        tmalloc_clear(Self->allocPtr);
//        tmalloc_releaseAllForward(Self->freePtr, NULL);
    } else {
//        tmalloc_releaseAllForward(Self->freePtr, NULL);
        XEND();
//        tmalloc_clear(Self->allocPtr);
    }
    return 1;
}

void TxAbort(Thread* Self) {
    if (Self->isFallback) {
        /* Clean up after an abort. Restore any modified locations. */
//        if (Self->LocalUndo.put != Self->LocalUndo.List) {
            WriteBackReverse(&Self->LocalUndo);
//        }
        
        // release global lock
        releaseLock(&globallock);

        ++Self->Retries;
        ++Self->Aborts;
        
        // Rollback any memory allocation, and longjmp to retry the txn
//        tmalloc_releaseAllReverse(Self->allocPtr, NULL);
//        tmalloc_clear(Self->freePtr);
        SIGLONGJMP(*Self->envPtr, 1);
        ASSERT(0);
    } else {
        XABORT(0);
    }
}
