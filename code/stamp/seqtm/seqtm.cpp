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
#include "../hytm1/platform_impl.h"
#include "seqtm.h"
#include "util.h"

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE           64
#endif

struct _Thread {
    long UniqID;
    volatile long Retries;
    int* ROFlag;
    int isFallback;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
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

volatile long StartTally = 0;
volatile long AbortTally = 0;

extern "C" void TxOnce() {
    printf("TM system ready\n");
}

extern "C" void TxShutdown() {
    printf("TM system shutdown:\n"
            "  Starts=%li Aborts=%li\n",
            StartTally, AbortTally);
}

extern "C" Thread* TxNewThread() {
    Thread* t = (Thread*) malloc(sizeof (Thread));
    assert(t);
    return t;
}

extern "C" void TxFreeThread(Thread* t) {
    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), t->Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTally)), t->Aborts);
    free(t);
}

extern "C" void TxInitThread(Thread* t, long id) {
    memset(t, 0, sizeof (*t)); /* Default value for most members */
    t->UniqID = id;
    t->rng = id + 1;
    t->xorrng[0] = t->rng;
}

template <typename T>
T templateTxLoad(Thread* Self, volatile T* addr) {
    return *addr;
}
template <typename T>
T templateTxStore(Thread* Self, volatile T* addr, T value) {
    return *addr = value;
}
template <typename T>
T templateTxStoreLocal(Thread* Self, volatile T* addr, T value) {
    return *addr = value;
}

extern "C" long TxLoadl(Thread* Self, volatile long* addr) {
    return templateTxLoad(Self, addr);
}
extern "C" intptr_t TxLoadp(Thread* Self, volatile intptr_t* addr) {
    return templateTxLoad(Self, addr);
}
extern "C" float TxLoadf(Thread* Self, volatile float* addr) {
    return templateTxLoad(Self, addr);
}

extern "C" long TxStorel(Thread* Self, volatile long* addr, long value) {
    return templateTxStore(Self, addr, value);
}
extern "C" intptr_t TxStorep(Thread* Self, volatile intptr_t* addr, intptr_t value) {
    return templateTxStore(Self, addr, value);
}
extern "C" float TxStoref(Thread* Self, volatile float* addr, float value) {
    return templateTxStore(Self, addr, value);
}

extern "C" long TxStoreLocall(Thread* Self, volatile long* addr, long value) {
    return templateTxStoreLocal(Self, addr, value);
}
extern "C" intptr_t TxStoreLocalp(Thread* Self, volatile intptr_t* addr, intptr_t value) {
    return templateTxStoreLocal(Self, addr, value);
}
extern "C" float TxStoreLocalf(Thread* Self, volatile float* addr, float value) {
    return templateTxStoreLocal(Self, addr, value);
}

extern "C" void TxStart(Thread* Self, sigjmp_buf* envPtr, int aborted_in_software, int* ROFlag) {
    Self->Starts++;
}

extern "C" int TxCommit(Thread* Self) {
    return 1;
}

extern "C" void TxAbort(Thread* Self) {

}
