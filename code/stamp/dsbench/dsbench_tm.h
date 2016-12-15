/* 
 * File:   dsbench_tm.h
 * Author: trbot
 *
 * Created on December 14, 2016, 4:01 PM
 */

#ifndef DSBENCH_TM_H
#define	DSBENCH_TM_H

#  include <stdio.h>

#  define MAIN(argc, argv)              int main (int argc, char** argv)
#  define MAIN_RETURN(val)              return val

#  define GOTO_SIM()                    /* nothing */
#  define GOTO_REAL()                   /* nothing */
#  define IS_IN_SIM()                   (0)

#  define SIM_GET_NUM_CPU(var)          /* nothing */

#  define TM_PRINTF                     printf
#  define TM_PRINT0                     printf
#  define TM_PRINT1                     printf
#  define TM_PRINT2                     printf
#  define TM_PRINT3                     printf

#  define P_MEMORY_STARTUP(numThread)   /* nothing */
#  define P_MEMORY_SHUTDOWN()           /* nothing */

#  include <string.h>
#  include "../lib/thread.h"

#    define TM_ARG                        STM_SELF,
#    define TM_ARG_ALONE                  STM_SELF
#    define TM_ARGDECL                    STM_THREAD_T* TM_ARG
#    define TM_ARGDECL_ALONE              STM_THREAD_T* TM_ARG_ALONE
#    define TM_CALLABLE                   /* nothing */

#      define TM_STARTUP(numThread)     STM_STARTUP()
#      define TM_SHUTDOWN()             STM_SHUTDOWN()

#      define TM_THREAD_ENTER()         TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                                        STM_INIT_THREAD(TM_ARG_ALONE, thread_getId())
#      define TM_THREAD_EXIT()          STM_FREE_THREAD(TM_ARG_ALONE)

#      define P_MALLOC(size)            malloc(size)
#      define P_FREE(ptr)               /*free(ptr)*/
#      define TM_MALLOC(size)           STM_MALLOC(size)
#      define TM_FREE(ptr)              /*STM_FREE(ptr)*/

#    define TM_BEGIN()                  STM_BEGIN_WR()
#    define TM_BEGIN_RO()               STM_BEGIN_RD()
#    define TM_END()                    STM_END()
#    define TM_RESTART()                STM_RESTART()

#    define TM_EARLY_RELEASE(var)       /* nothing */

#  define TM_SHARED_READ_L(var)         STM_READ_L(var)
#  define TM_SHARED_READ_P(var)         STM_READ_P(var)
#  define TM_SHARED_READ_F(var)         STM_READ_F(var)

#  define TM_SHARED_WRITE_L(var, val)   STM_WRITE_L((var), val)
#  define TM_SHARED_WRITE_P(var, val)   STM_WRITE_P((var), val)
#  define TM_SHARED_WRITE_F(var, val)   STM_WRITE_F((var), val)

#  define TM_LOCAL_WRITE_L(var, val)    STM_LOCAL_WRITE_L(var, val)
#  define TM_LOCAL_WRITE_P(var, val)    STM_LOCAL_WRITE_P(var, val)
#  define TM_LOCAL_WRITE_F(var, val)    STM_LOCAL_WRITE_F(var, val)

#  define TM_SHARED_READ_I(var)         TM_SHARED_READ_L(var)
#  define TM_SHARED_WRITE_I(var, val)   TM_SHARED_WRITE_L(var, val)
#  define TM_LOCAL_WRITE_I(var, val)    TM_LOCAL_WRITE_L(var, val)

#if defined hytm1
#include "../hytm1/stm.h"
#include "../hytm1/tmalloc.h"
#include "../hytm1/tmalloc.c"
#include "../hytm1/hytm1.h"
#include "../hytm1/hytm1.cpp"
#warning Using hytm1
#elif defined hytm2
#include "../hytm2/stm.h"
#include "../hytm2/tmalloc.h"
#include "../hytm2/tmalloc.c"
#include "../hytm2/hytm2.h"
#include "../hytm2/hytm2.cpp"
#warning Using hytm2
#elif defined hytm2sw
#error hytm2sw is not supported
#elif defined hytm3
#include "../hytm3/stm.h"
#include "../hytm3/tmalloc.h"
#include "../hytm3/tmalloc.c"
#include "../hytm3/hytm3.h"
#include "../hytm3/hytm3.cpp"
#warning Using hytm3
#elif defined hybridnorec
#include "../hybridnorec/stm.h"
#include "../hybridnorec/tmalloc.h"
#include "../hybridnorec/tmalloc.c"
#include "../hybridnorec/hybridnorec.h"
#include "../hybridnorec/hybridnorec.cpp"
#warning Using hybridnorec
#elif defined tl2
#include "../tl2/stm.h"
#include "../tl2/tmalloc.h"
#include "../tl2/tmalloc.c"
#include "../tl2/tl2.h"
#include "../tl2/tl2.c"
#warning Using tl2
#elif defined seqtm
#include "../seqtm/stm.h"
#include "../seqtm/seqtm.h"
#include "../seqtm/seqtm.cpp"
#warning Using seqtm
#elif defined TLE
#include "tlewrapper/stm.h"
#include "tle.h"
#warning Using TLE
#endif

#endif	/* DSBENCH_TM_H */
