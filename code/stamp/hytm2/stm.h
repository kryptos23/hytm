/* =============================================================================
 *
 * stm.h
 *
 * User program interface for STM. For an STM to interface with STAMP, it needs
 * to have its own stm.h for which it redefines the macros appropriately.
 *
 * =============================================================================
 *
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * Edited by Trevor Brown (tabrown@cs.toronto.edu)
 *
 * =============================================================================
 */


#ifndef STM_H
#define STM_H 1

#  include <setjmp.h>
#include <stdio.h>
#include "recordmgr/machineconstants.h"

extern volatile long CommitTallySW;

typedef struct Thread_void {
    PAD;
    long UniqID;
    volatile long Retries;
//    int* ROFlag; // not used by stamp
    int IsRO;
    int isFallback;
//    long Starts; // how many times the user called TxBegin
    long AbortsHW; // # of times hw txns aborted
    long AbortsSW; // # of times sw txns aborted
    long CommitsHW;
    long CommitsSW;
    unsigned long long rng;
    unsigned long long xorrng [1];
    void* allocPtr;    /* CCM: speculatively allocated */
    void* freePtr;     /* CCM: speculatively free'd */
    void* rdSet;
    void* wrSet;
//    void* LocalUndo;
    sigjmp_buf* envPtr;
    PAD;
} Thread_void;

#include "hytm2.h"
#include "util.h"

#define STM_THREAD_T                    void
#define STM_SELF                        Self
#define STM_RO_FLAG                     ROFlag

#define STM_MALLOC(size)                TxAlloc(STM_SELF, size) /*malloc(size)*/
#define STM_FREE(ptr)                   TxFree(STM_SELF, ptr)

#  define STM_JMPBUF_T                  sigjmp_buf
#  define STM_JMPBUF                    buf

#define STM_CLEAR_COUNTERS()            TxClearCounters()
#define STM_VALID()                     (1)
#define STM_RESTART()                   TxAbort(STM_SELF); /*{ Thread_void *___self = (Thread_void *) STM_SELF; if (___self->isFallback) TxAbort(STM_SELF); else XABORT(3); }*/

#define STM_STARTUP()                   TxOnce()
#define STM_SHUTDOWN()                  TxShutdown()

#define STM_NEW_THREAD(id)              TxNewThread()
#define STM_INIT_THREAD(t, id)          TxInitThread(t, id)
#define STM_FREE_THREAD(t)              TxFreeThread(t)

__thread intptr_t (*sharedReadFunPtr)(void* Self, volatile intptr_t* addr);
__thread void (*sharedWriteFunPtr)(void* Self, volatile intptr_t* addr, intptr_t val);

#  define STM_BEGIN(isReadOnly)         do { \
                                            SOFTWARE_BARRIER; \
                                            sharedReadFunPtr = &TxLoad_htm; \
                                            sharedWriteFunPtr = &TxStore_htm; \
                                            SOFTWARE_BARRIER; \
                                            STM_JMPBUF_T STM_JMPBUF; \
                                            /*int STM_RO_FLAG = isReadOnly;*/ \
                                            \
                                            Thread_void* ___Self = (Thread_void*) STM_SELF; \
                                            TxClearRWSets(STM_SELF); \
                                            \
                                            XBEGIN_ARG_T ___xarg; \
                                            ___Self->Retries = 0; \
                                            ___Self->isFallback = 0; \
                                            ___Self->IsRO = 1; \
                                            ___Self->envPtr = &STM_JMPBUF; \
                                            unsigned ___htmattempts; \
                                            for (___htmattempts = 0; ___htmattempts < HTM_ATTEMPT_THRESH; ++___htmattempts) { \
                                                if (XBEGIN(___xarg)) { \
                                                    break; \
                                                } else { /* if we aborted */ \
                                                    TM_REGISTER_ABORT(PATH_FAST_HTM, ___xarg, ___Self->UniqID); \
                                                    ++___Self->AbortsHW; \
                                                } \
                                            } \
                                            /*printf("exited loop\n");*/ \
                                            if (___htmattempts < HTM_ATTEMPT_THRESH) break; \
                                            /* STM attempt */ \
                                            /*DEBUG2 aout("thread "<<___Self->UniqID<<" started s/w tx attempt "<<(___Self->AbortsSW+___Self->CommitsSW)<<"; s/w commits so far="<<___Self->CommitsSW);*/ \
                                            /*DEBUG1 if ((___Self->CommitsSW % 50000) == 0) aout("thread "<<___Self->UniqID<<" has committed "<<___Self->CommitsSW<<" s/w txns");*/ \
                                            DEBUG2 printf("thread %ld started s/w tx; attempts so far=%ld, s/w commits so far=%ld\n", ___Self->UniqID, (___Self->AbortsSW+___Self->CommitsSW), ___Self->CommitsSW); \
                                            DEBUG1 if ((___Self->CommitsSW % 25000) == 0) printf("thread %ld has committed %ld s/w txns (over all threads so far=%ld)\n", ___Self->UniqID, ___Self->CommitsSW, CommitTallySW); \
                                            if (sigsetjmp(STM_JMPBUF, 1)) { \
                                                TxClearRWSets(STM_SELF); \
                                            } \
                                            SOFTWARE_BARRIER; \
                                            sharedReadFunPtr = &TxLoad_stm; \
                                            sharedWriteFunPtr = &TxStore_stm; \
                                            SOFTWARE_BARRIER; \
                                            ___Self->isFallback = 1; \
                                            ___Self->IsRO = 1; \
                                            SYNC_RMW; /* prevent instructions in the txn/critical section from being moved before this point (on power) */ \
                                            SOFTWARE_BARRIER; \
                                        } while (0); /* enforce comma */


#define STM_BEGIN_RD()                  STM_BEGIN(1)
#define STM_BEGIN_WR()                  STM_BEGIN(0)
#define STM_END()                       SOFTWARE_BARRIER; TxCommit(STM_SELF)


typedef volatile intptr_t               vintp;

#if 0
#define STM_READ_L(var)                 TxLoad(STM_SELF, (vintp*)(void*)&(var))
#define STM_READ_F(var)                 IP2F(TxLoad(STM_SELF, \
                                                    (vintp*)FP2IPP(&(var))))
#define STM_READ_P(var)                 IP2VP(TxLoad(STM_SELF, \
                                                     (vintp*)(void*)&(var)))

#define STM_WRITE_L(var, val)           TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                (intptr_t)(val))
/**
 * the following casts do not work when compiled in x64,
 * since typically 2*sizeof(float) == sizeof(intptr).
 * consequently, writing to a float also writes to the
 * adjacent float...
 */
#define STM_WRITE_F(var, val)           TxStore(STM_SELF, \
                                                (vintp*)FP2IPP(&(var)), \
                                                F2IP(val))
#define STM_WRITE_P(var, val)           TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                VP2IP(val))
#else
#define STM_READ_L(var)                 (*sharedReadFunPtr)(STM_SELF, (vintp*)(void*)&(var))
#define STM_READ_F(var)                 IP2F((*sharedReadFunPtr)(STM_SELF, \
                                                    (vintp*)FP2IPP(&(var))))
#define STM_READ_P(var)                 IP2VP((*sharedReadFunPtr)(STM_SELF, \
                                                     (vintp*)(void*)&(var)))
#define STM_WRITE_L(var, val)           (*sharedWriteFunPtr)(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                (intptr_t)(val))
#define STM_WRITE_F(var, val)           (*sharedWriteFunPtr)(STM_SELF, \
                                                (vintp*)FP2IPP(&(var)), \
                                                F2IP(val))
#define STM_WRITE_P(var, val)           (*sharedWriteFunPtr)(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                VP2IP(val))
#endif

#define STM_LOCAL_WRITE_L(var, val)     ({var = val; var;})
#define STM_LOCAL_WRITE_F(var, val)     ({var = val; var;})
#define STM_LOCAL_WRITE_P(var, val)     ({var = val; var;})
//*/

#endif /* STM_H */


/* =============================================================================
 *
 * End of stm.h
 *
 * =============================================================================
 */
