#ifndef HYTM1_H
#define HYTM1_H 1

#include <stdint.h>
#include "rtm.h"
//#include "tmalloc.h"

//enum hytm1_path { FALLBACK, FAST };

typedef struct _Thread Thread;

#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)

/*
 * Prototypes
 */

void     TxStart       (Thread*, sigjmp_buf*, int, int*);
Thread*  TxNewThread   ();

void     TxFreeThread  (Thread*);
void     TxInitThread  (Thread*, long id);
int      TxCommit      (Thread*);
void     TxAbort       (Thread*);
intptr_t TxLoad        (Thread*, volatile intptr_t*);
void     TxStore       (Thread*, volatile intptr_t*, intptr_t);
void     TxStoreLocal  (Thread*, volatile intptr_t*, intptr_t);
void     TxOnce        ();
void     TxShutdown    ();

void*    TxAlloc       (Thread*, size_t);
void     TxFree        (Thread*, void*);

#endif /* HYTM1_H */
