#ifndef HYTM2_H
#define HYTM2_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rtm.h"
#include "tmalloc.h"

#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)

/*
 * Prototypes
 */

void     TxStart       (void*, sigjmp_buf*, int, int*);
void*    TxNewThread   ();

void     TxFreeThread  (void*);
void     TxInitThread  (void*, long id);
int      TxCommit      (void*);
void     TxAbort       (void*);

//intptr_t TxLoad(void* Self, volatile intptr_t* addr);
//void TxStore(void* Self, volatile intptr_t* addr, intptr_t value);

long TxLoadl(void* Self, volatile long* addr);
//intptr_t TxLoadp(void* Self, volatile intptr_t* addr);
float TxLoadf(void* Self, volatile float* addr);

long TxStorel(void* Self, volatile long* addr, long value);
//intptr_t TxStorep(void* Self, volatile intptr_t* addr, intptr_t value);
float TxStoref(void* Self, volatile float* addr, float value);

//long TxStoreLocall(void* Self, volatile long* addr, long value);
//intptr_t TxStoreLocalp(void* Self, volatile intptr_t* addr, intptr_t value);
//float TxStoreLocalf(void* Self, volatile float* addr, float value);

void     TxOnce        ();
void     TxShutdown    ();

void*    TxAlloc       (void*, size_t);
void     TxFree        (void*, void*);

#ifdef __cplusplus
}
#endif

#endif
