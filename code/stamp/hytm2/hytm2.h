#ifndef HYTM2_H
#define HYTM2_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define TM_NAME "HyTM2"
//#define HTM_ATTEMPT_THRESH 0
#ifndef HTM_ATTEMPT_THRESH
    #define HTM_ATTEMPT_THRESH 5
#endif
#define TXNL_MEM_RECLAMATION

#define MAX_RETRIES 100000
    
#include "../hytm1/counters/debugcounters.h"
extern struct debugCounters *counters;

//#define DEBUG_PRINT
#define DEBUG_PRINT_LOCK

#define DEBUG0 if(0)
#define DEBUG1 DEBUG0 if(1)
#define DEBUG2 DEBUG1 if(0)
#define DEBUG3 DEBUG2 if(0)

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

#define debug(x) (#x)<<"="<<x
//#define LONG_VALIDATION
#define VALIDATE_INV(x) VALIDATE (x)->validateInvariants()
#define VALIDATE if(0)
#define ERROR(x) { \
    cerr<<"ERROR: "<<x<<endl; \
    printStackTrace(); \
    exit(-1); \
}

// just for debugging
extern volatile int globallock;

#define BIG_CONSTANT(x) (x##LLU)





    
#include <stdint.h>
#include "rtm.h"
#include "tmalloc.h"

#  include <setjmp.h>
#  define SIGSETJMP(env, savesigs)      sigsetjmp(env, savesigs)
#  define SIGLONGJMP(env, val)          siglongjmp(env, val); assert(0)

/*
 * Prototypes
 */

void     TxClearRWSets (void* _Self);
void     TxStart       (void*, sigjmp_buf*, int, int*);
void*    TxNewThread   ();

void     TxFreeThread  (void*);
void     TxInitThread  (void*, long id);
int      TxCommit      (void*);
void     TxAbort       (void*);

intptr_t TxLoad(void* Self, volatile intptr_t* addr);
void TxStore(void* Self, volatile intptr_t* addr, intptr_t value);

//long TxLoadl(void* Self, volatile long* addr);
////intptr_t TxLoadp(void* Self, volatile intptr_t* addr);
//float TxLoadf(void* Self, volatile float* addr);
//
//long TxStorel(void* Self, volatile long* addr, long value);
////intptr_t TxStorep(void* Self, volatile intptr_t* addr, intptr_t value);
//float TxStoref(void* Self, volatile float* addr, float value);
//
////long TxStoreLocall(void* Self, volatile long* addr, long value);
////intptr_t TxStoreLocalp(void* Self, volatile intptr_t* addr, intptr_t value);
////float TxStoreLocalf(void* Self, volatile float* addr, float value);

void     TxOnce        ();
void     TxShutdown    ();

void*    TxAlloc       (void*, size_t);
void     TxFree        (void*, void*);

#ifdef __cplusplus
}
#endif

#endif
