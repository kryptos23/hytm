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


#include "seqtm.h"
#include "util.h"

#define STM_THREAD_T                    Thread
#define STM_SELF                        Self
#define STM_RO_FLAG                     ROFlag

#define STM_MALLOC(size)                malloc(size) /*TxAlloc(STM_SELF, size)*/
#define STM_FREE(ptr)                   /*TxFree(STM_SELF, ptr)*/

#  include <setjmp.h>
#  define STM_JMPBUF_T                  sigjmp_buf
#  define STM_JMPBUF                    buf


#define STM_VALID()                     (1)
#define STM_RESTART()                   TxAbort(STM_SELF)

#define STM_STARTUP()                   TxOnce()
#define STM_SHUTDOWN()                  TxShutdown()

#define STM_NEW_THREAD()                TxNewThread()
#define STM_INIT_THREAD(t, id)          TxInitThread(t, id)
#define STM_FREE_THREAD(t)              TxFreeThread(t)








#  define STM_BEGIN(isReadOnly)         do { \
                                            STM_JMPBUF_T STM_JMPBUF; \
                                            int STM_RO_FLAG = isReadOnly; \
                                            int SETJMP_RETVAL = 0; /*sigsetjmp(STM_JMPBUF, 1);*/ \
                                            TxStart(STM_SELF, &STM_JMPBUF, SETJMP_RETVAL, &STM_RO_FLAG); \
                                        } while (0); /* enforce comma */

#define STM_BEGIN_RD()                  STM_BEGIN(1)
#define STM_BEGIN_WR()                  STM_BEGIN(0)
#define STM_END()                       TxCommit(STM_SELF)

typedef volatile intptr_t               vintp;

#if(0)
#define STM_READ_L(var)                 var /*TxLoad(STM_SELF, (vintp*)(void*)&(var))*/
#define STM_READ_F(var)                 var /*IP2F(TxLoad(STM_SELF, \
                                                    (vintp*)FP2IPP(&(var))))*/
#define STM_READ_P(var)                 var /*IP2VP(TxLoad(STM_SELF, \
                                                     (vintp*)(void*)&(var)))*/
#else
#define STM_READ_L(var)                 TxLoadl(STM_SELF, (volatile long*)(void*)&(var))
#define STM_READ_F(var)                 TxLoadf(STM_SELF, (volatile float*)&(var))
#define STM_READ_P(var)                 ((void*) TxLoadp(STM_SELF, (vintp*)(void*)&(var)))
#endif




#if(0)
#define STM_WRITE_L(var, val)           var = val /*TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                (intptr_t)(val))*/
#define STM_WRITE_F(var, val)           var = val /*TxStore(STM_SELF, \
                                                (vintp*)FP2IPP(&(var)), \
                                                F2IP(val))*/
#define STM_WRITE_P(var, val)           var = val /*TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                VP2IP(val))*/
#else
#define STM_WRITE_L(var, val)           TxStorel(STM_SELF, (volatile long*)(void*)&(var), (long)(val))
#define STM_WRITE_F(var, val)           TxStoref(STM_SELF, (volatile float*)&(var), (float)(val))
#define STM_WRITE_P(var, val)           TxStorep(STM_SELF, (volatile intptr_t*)&(var), (volatile intptr_t)(void*)(val))
#endif




#define STM_LOCAL_WRITE_L(var, val)     TxStoreLocall(STM_SELF, (volatile long*)(void*)&(var), (long)(val))
#define STM_LOCAL_WRITE_F(var, val)     TxStoreLocalf(STM_SELF, (volatile float*)&(var), (float)(val))
#define STM_LOCAL_WRITE_P(var, val)     TxStoreLocalp(STM_SELF, (volatile intptr_t*)&(var), (volatile intptr_t)(void*)(val))


#endif /* STM_H */


/* =============================================================================
 *
 * End of stm.h
 *
 * =============================================================================
 */
