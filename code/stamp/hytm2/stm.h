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


#include "hytm2.h"
#include "util.h"

#define STM_THREAD_T                    void
#define STM_SELF                        Self
#define STM_RO_FLAG                     ROFlag

#define STM_MALLOC(size)                TxAlloc(STM_SELF, size) /*malloc(size)*/
#define STM_FREE(ptr)                   TxFree(STM_SELF, ptr)

#  include <setjmp.h>
#  define STM_JMPBUF_T                  sigjmp_buf
#  define STM_JMPBUF                    buf


#define STM_VALID()                     (1)
#define STM_RESTART()                   TxAbort(STM_SELF)

#define STM_STARTUP()                   TxOnce()
#define STM_SHUTDOWN()                  TxShutdown()

#define STM_NEW_THREAD(id)              TxNewThread()
#define STM_INIT_THREAD(t, id)          TxInitThread(t, id)
#define STM_FREE_THREAD(t)              TxFreeThread(t)








#  define STM_BEGIN(isReadOnly)         do { \
                                            STM_JMPBUF_T STM_JMPBUF; \
                                            int STM_RO_FLAG = isReadOnly; \
                                            int SETJMP_RETVAL = sigsetjmp(STM_JMPBUF, 1); \
                                            TxStart(STM_SELF, &STM_JMPBUF, SETJMP_RETVAL, &STM_RO_FLAG); \
                                        } while (0); /* enforce comma */

#define STM_BEGIN_RD()                  STM_BEGIN(1)
#define STM_BEGIN_WR()                  STM_BEGIN(0)
#define STM_END()                       TxCommit(STM_SELF)

/*
typedef volatile intptr_t               vintp;

#define STM_READ(var)                   TxLoad(STM_SELF, (vintp*)(void*)&(var))
#define STM_READ_F(var)                 IP2F(TxLoad(STM_SELF, \
                                                    (vintp*)FP2IPP(&(var))))
#define STM_READ_P(var)                 IP2VP(TxLoad(STM_SELF, \
                                                     (vintp*)(void*)&(var)))

#define STM_WRITE(var, val)             TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                (intptr_t)(val))
#define STM_WRITE_F(var, val)           TxStore(STM_SELF, \
                                                (vintp*)FP2IPP(&(var)), \
                                                F2IP(val))
#define STM_WRITE_P(var, val)           TxStore(STM_SELF, \
                                                (vintp*)(void*)&(var), \
                                                VP2IP(val))
*/
#define STM_LOCAL_WRITE_L(var, val)     ({var = val; var;})
#define STM_LOCAL_WRITE_F(var, val)     ({var = val; var;})
#define STM_LOCAL_WRITE_P(var, val)     ({var = val; var;})
//*/

#define STM_READ_L(var)                 TxLoadl(STM_SELF, (volatile long*)(void*)&(var))
#define STM_READ_F(var)                 TxLoadf(STM_SELF, (volatile float*)&(var))
#define STM_READ_P(var)                 TxLoadl(STM_SELF, (volatile intptr_t*)(void*)&(var))
#define STM_WRITE_L(var, val)           TxStorel(STM_SELF, (volatile long*)(void*)&(var), (long)(val))
#define STM_WRITE_F(var, val)           TxStoref(STM_SELF, (volatile float*)&(var), (float)(val))
#define STM_WRITE_P(var, val)           TxStorel(STM_SELF, (volatile intptr_t*)&(var), (volatile intptr_t)(void*)(val))
/*
#define STM_LOCAL_WRITE_L(var, val)     TxStoreLocall(STM_SELF, (volatile long*)(void*)&(var), (long)(val))
#define STM_LOCAL_WRITE_F(var, val)     TxStoreLocalf(STM_SELF, (volatile float*)&(var), (float)(val))
#define STM_LOCAL_WRITE_P(var, val)     TxStoreLocalp(STM_SELF, (volatile intptr_t*)&(var), (volatile intptr_t)(void*)(val))
//*/

//#define STM_READ_L(var)                 IP2L(TxLoad(STM_SELF, LP2IPP(&(var))))
//#define STM_READ_F(var)                 IP2F(TxLoad(STM_SELF, FP2IPP(&(var))))
//#define STM_READ_P(var)                 IP2VP(TxLoad(STM_SELF, (volatile intptr_t*)(void*)&(var)))
//#define STM_WRITE_L(var, val)           TxStore(STM_SELF, LP2IPP(&(var)), L2IP((val)))
//#define STM_WRITE_F(var, val)           TxStore(STM_SELF, FP2IPP(&(var)), F2IP((val)))
//#define STM_WRITE_P(var, val)           TxStore(STM_SELF, (volatile intptr_t*)&(var), (volatile intptr_t)(void*)(val))


#endif /* STM_H */


/* =============================================================================
 *
 * End of stm.h
 *
 * =============================================================================
 */
