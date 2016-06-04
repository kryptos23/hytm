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

#define STM_THREAD_T                    void
#define STM_SELF                        Self
#define STM_RO_FLAG                     ROFlag

#define STM_MALLOC(size)                
#define STM_FREE(ptr)                   

#  define STM_JMPBUF_T                  void*
#  define STM_JMPBUF                    buf


#define STM_VALID()                     (1)
#define STM_RESTART()                   

#define STM_STARTUP()                   
#define STM_SHUTDOWN()                  

#define STM_NEW_THREAD(id)              NULL
#define STM_INIT_THREAD(t, id)          
#define STM_FREE_THREAD(t)              

#  define STM_BEGIN(isReadOnly)

#define STM_BEGIN_RD()                  
#define STM_BEGIN_WR()                  
#define STM_END()                       


typedef volatile intptr_t               vintp;

#define STM_READ_L(var)                 0
#define STM_READ_F(var)                 0
#define STM_READ_P(var)                 NULL

#define STM_WRITE_L(var, val)           0
#define STM_WRITE_F(var, val)           0
#define STM_WRITE_P(var, val)           NULL

#define STM_LOCAL_WRITE_L(var, val)     0
#define STM_LOCAL_WRITE_F(var, val)     0
#define STM_LOCAL_WRITE_P(var, val)     NULL

#endif /* STM_H */