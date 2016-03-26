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
//#include "tmalloc.h"
#include "util.h"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE           64
#endif

enum hytm1_config {
    INIT_WRSET_NUM_ENTRY = 1024,
    INIT_RDSET_NUM_ENTRY = 8192,
    INIT_LOCAL_NUM_ENTRY = 1024,
};

#define HTM_RETRY_THRESH          100

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
    volatile intptr_t* Addr;
    intptr_t Valu;
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

struct _Thread {
    long UniqID;
    volatile long Retries;
    int* ROFlag;
//    int IsRO; // skip so that first 8 variables fit in a single cache line
//    hytm1_path path;
    int isFallback;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
//    tmalloc_t* allocPtr; /* CCM: speculatively allocated */
//    tmalloc_t* freePtr; /* CCM: speculatively free'd */
    Log rdSet;
    Log wrSet;
    Log LocalUndo;
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
__INLINE__ void WriteBackForward(Log* k) {
    AVPair* e;
    AVPair* End = k->put;
    for (e = k->List; e != End; e = e->Next) {
        *(e->Addr) = e->Valu;
    }
}

/* =============================================================================
 * WriteBackReverse
 *
 * Transfer the data in the log to its ultimate location.
 * =============================================================================
 */
__INLINE__ void WriteBackReverse(Log* k) {
    AVPair* e;
    for (e = k->tail; e != NULL; e = e->Prev) {
        *(e->Addr) = e->Valu;
    }
}

/* =============================================================================
 * FindFirst
 *
 * Search for first log entry that contains lock.
 * =============================================================================
 */
__INLINE__ AVPair* FindFirst(Log* k, volatile vwLock* Lock) {
    AVPair* e;
    AVPair * const End = k->put;
    for (e = k->List; e != End; e = e->Next) {
        if (e->LockFor == Lock) {
            return e;
        }
    }
    return NULL;
}

// for redo log: deferred writes -> write-back phase
__INLINE__ void RecordStore(Log* k, volatile intptr_t* Addr, intptr_t Valu, volatile vwLock* Lock) {
    /*
     * As an optimization we could squash multiple stores to the same location.
     * Maintain FIFO order to avoid WAW hazards.
     * TODO-FIXME - CONSIDER
     * Keep Self->LockSet as a sorted linked list of unique LockFor addresses.
     * We'd scan the LockSet for Lock.  If not found we'd insert a new
     * LockRecord at the appropriate location in the list.
     * Call InsertIfAbsent (Self, LockFor)
     */
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    ASSERT(Addr != NULL);
    k->tail = e;
    k->put = e->Next;
    e->Addr = Addr;
    e->Valu = Valu;
    e->LockFor = Lock;
    //    e->Held    = 0;
    e->rdv = 0; //LOCKBIT; /* use either 0 or LOCKBIT */
}

// for undo log: immediate writes -> undo on abort/restart
__INLINE__ void SaveForRollBack(Log* k, volatile intptr_t* Addr, intptr_t Valu) {
    AVPair* e = k->put;
    if (e == NULL) {
        k->ovf++;
        e = ExtendList(k->tail);
        k->end = e;
    }
    k->tail = e;
    k->put = e->Next;
    e->Addr = Addr;
    e->Valu = Valu;
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
//    AtomicAdd((volatile intptr_t*)((void*) (&ReadOverflowTally)), t->rdSet.ovf);
//    long wrSetOvf = 0;
//    Log* wr;
//    wr = &t->wrSet;
//    {
//        wrSetOvf += wr->ovf;
//    }
//    AtomicAdd((volatile intptr_t*)((void*) (&WriteOverflowTally)), wrSetOvf);
//    AtomicAdd((volatile intptr_t*)((void*) (&LocalOverflowTally)), t->LocalUndo.ovf);
    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), t->Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTally)), t->Aborts);

//    tmalloc_free(t->allocPtr);
//    tmalloc_free(t->freePtr);

    FreeList(&(t->rdSet), INIT_RDSET_NUM_ENTRY);
    FreeList(&(t->wrSet), INIT_WRSET_NUM_ENTRY);
    FreeList(&(t->LocalUndo), INIT_LOCAL_NUM_ENTRY);

    free(t);
}

void TxInitThread(Thread* t, long id) {
    memset(t, 0, sizeof (*t)); /* Default value for most members */

    t->UniqID = id;
    t->rng = id + 1;
    t->xorrng[0] = t->rng;

    t->wrSet.List = MakeList(INIT_WRSET_NUM_ENTRY, t);
    t->wrSet.put = t->wrSet.List;

    t->rdSet.List = MakeList(INIT_RDSET_NUM_ENTRY, t);
    t->rdSet.put = t->rdSet.List;

    t->LocalUndo.List = MakeList(INIT_LOCAL_NUM_ENTRY, t);
    t->LocalUndo.put = t->LocalUndo.List;

//    t->allocPtr = tmalloc_alloc(1);
//    assert(t->allocPtr);
//    t->freePtr = tmalloc_alloc(1);
//    assert(t->freePtr);

}

__INLINE__ void txReset(Thread* Self) {
    Self->wrSet.put = Self->wrSet.List;
    Self->wrSet.tail = NULL;

    Self->rdSet.put = Self->rdSet.List;
    Self->rdSet.tail = NULL;

    Self->LocalUndo.put = Self->LocalUndo.List;
    Self->LocalUndo.tail = NULL;
}

intptr_t TxLoad(Thread* Self, volatile intptr_t* Addr) {
    return *Addr;
//    intptr_t Valu;
//
//    /*
//     * Preserve the illusion of processor consistency in run-ahead mode.
//     * Look-aside: check the wrSet for RAW hazards.
//     * This is optional, but it improves the quality and fidelity
//     * of the wrset and rdset compiled during speculative mode.
//     * Consider using a Bloom filter on the addresses in wrSet to give us
//     * a statistically fast out if the address doesn't appear in the set
//     */
//
//    intptr_t msk = FILTERBITS(Addr);
//    if ((Self->wrSet.BloomFilter & msk) == msk) {
//        Log* wr = &Self->wrSet;
//        AVPair* e;
//        for (e = wr->tail; e != NULL; e = e->Prev) {
//            ASSERT(e->Addr != NULL);
//            if (e->Addr == Addr) {
//                return e->Valu;
//            }
//        }
//    }
//
//    /*
//     * TODO-FIXME:
//     * Currently we set Self->rv in TxStart(). We might be better served to
//     * defer reading Self->rv until the 1st transactional load.
//     * if (Self->rv == 0) Self->rv = _GCLOCK
//     */
//
//    /*
//     * Fetch tentative value
//     * Use either SPARC non-fault loads or complicit signal handlers.
//     * If the LD fails we'd like to call TxAbort()
//     * TL2 does not permit zombie/doomed txns to run
//     */
//    volatile vwLock* LockFor = PSLOCK(Addr);
//    vwLock rdv = LDLOCK(LockFor) & ~LOCKBIT;
//    MEMBARLDLD();
//    Valu = LDNF(Addr);
//    MEMBARLDLD();
//    if (rdv <= Self->rv && LDLOCK(LockFor) == rdv) {
//        if (!Self->IsRO) {
//            if (!TrackLoad(Self, LockFor)) {
//                TxAbort(Self);
//            }
//        }
//        return Valu;
//    }
//
//    /*
//     * The location is either currently locked or has been updated since this
//     * txn started.  In the later case if the read-set is otherwise empty we
//     * could simply re-load Self->rv = _GCLOCK and try again.  If the location
//     * is locked it's fairly likely that the owner will release the lock by
//     * writing a versioned write-lock value that is > Self->rv, so spinning
//     * provides little profit.
//     */
//
//    Self->abv = rdv;
//    TxAbort(Self);
//    ASSERT(0);
//
//    return 0;
}

void TxStore(Thread* Self, volatile intptr_t* addr, intptr_t valu) {
    if (Self->isFallback) {
        SaveForRollBack(&Self->LocalUndo, addr, *addr);
    }
    *addr = valu;
//    volatile vwLock* LockFor;
//    vwLock rdv;
//
//    /*
//     * In TL2 we're always coherent, so we should never see NULL stores.
//     * In TL it was possible to see NULL stores in zombie txns.
//     */
//
//    //    ASSERT(Self->Mode == TTXN);
//    if (Self->IsRO) {
//        *(Self->ROFlag) = 0;
//        TxAbort(Self);
//        return;
//    }
//
//    LockFor = PSLOCK(addr);
//
//    /*
//     * CONSIDER: spin briefly (bounded) while the object is locked,
//     * periodically calling ReadSetCoherent(Self)
//     */
//
//    rdv = LDLOCK(LockFor);
//
//    Log* wr = &Self->wrSet;
//    /*
//     * Convert a redundant "idempotent" store to a tracked load.
//     * This helps minimize the wrSet size and reduces false+ aborts.
//     * Conceptually, "a = x" is equivalent to "if (a != x) a = x"
//     * This is entirely optional
//     */
//    MEMBARLDLD();
//
//    if (ALWAYS && LDNF(addr) == valu) {
//        AVPair* e;
//        for (e = wr->tail; e != NULL; e = e->Prev) {
//            ASSERT(e->Addr != NULL);
//            if (e->Addr == addr) {
//                ASSERT(LockFor == e->LockFor);
//                e->Valu = valu; /* CCM: update associated value in write-set */
//                return;
//            }
//        }
//        /* CCM: Not writing new value; convert to load */
//        if ((rdv & LOCKBIT) == 0 && rdv <= Self->rv && LDLOCK(LockFor) == rdv) {
//            if (!TrackLoad(Self, LockFor)) {
//                TxAbort(Self);
//            }
//            return;
//        }
//    }
//
//    wr->BloomFilter |= FILTERBITS(addr);
//
//    RecordStore(wr, addr, valu, LockFor);
}

/* =============================================================================
 * TxStoreLocal
 *
 * Update in-place, saving the original contents in the undo log
 * =============================================================================
 */
void TxStoreLocal(Thread* Self, volatile intptr_t* Addr, intptr_t Valu) {
    if (Self->isFallback) {
        SaveForRollBack(&Self->LocalUndo, Addr, *Addr);
    }
    *Addr = Valu;
}

void TxStart(Thread* Self, sigjmp_buf* envPtr, int aborted_in_software, int* ROFlag) {
    txReset(Self);
    Self->Retries = 0;
    Self->isFallback = 0;
    Self->ROFlag = ROFlag;
//    Self->IsRO = ROFlag ? *ROFlag : 0;
    Self->envPtr = envPtr;
    ASSERT(Self->LocalUndo.put == Self->LocalUndo.List);
    ASSERT(Self->wrSet.put == Self->wrSet.List);
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
        if (Self->LocalUndo.put != Self->LocalUndo.List) {
            WriteBackReverse(&Self->LocalUndo);
        }
        
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

//void* TxAlloc(Thread* Self, size_t size) {
//    void* ptr = tmalloc_reserve(size);
//    if (ptr) {
//        tmalloc_append(Self->allocPtr, ptr);
//    }
//
//    return ptr;
//}
//
//void TxFree(Thread* Self, void* ptr) {
//    tmalloc_append(Self->freePtr, ptr);
//}
