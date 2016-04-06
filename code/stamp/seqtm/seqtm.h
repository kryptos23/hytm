#ifndef HYTM1_H
#define HYTM1_H 1

#include <stdint.h>

typedef struct _Thread Thread;

#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)

/*
 * Prototypes
 */

#ifdef __cplusplus
extern "C" {
#endif

void     TxStart       (Thread*, sigjmp_buf*, int, int*);
Thread*  TxNewThread   ();

void     TxFreeThread  (Thread*);
void     TxInitThread  (Thread*, long id);
int      TxCommit      (Thread*);
void     TxAbort       (Thread*);

long TxLoadl(Thread* Self, volatile long* addr);
intptr_t TxLoadp(Thread* Self, volatile intptr_t* addr);
float TxLoadf(Thread* Self, volatile float* addr);

long TxStorel(Thread* Self, volatile long* addr, long value);
intptr_t TxStorep(Thread* Self, volatile intptr_t* addr, intptr_t value);
float TxStoref(Thread* Self, volatile float* addr, float value);

long TxStoreLocall(Thread* Self, volatile long* addr, long value);
intptr_t TxStoreLocalp(Thread* Self, volatile intptr_t* addr, intptr_t value);
float TxStoreLocalf(Thread* Self, volatile float* addr, float value);

void     TxOnce        ();
void     TxShutdown    ();

void*    TxAlloc       (Thread*, size_t);
void     TxFree        (Thread*, void*);

#ifdef __cplusplus
}
#endif

#endif /* HYTM1_H */
