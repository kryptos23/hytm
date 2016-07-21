#ifndef _RTM_H
#define _RTM_H 1

#include <htmxlintrin.h>

//     long __TM_simple_begin (void)
//     long __TM_begin (void* const TM_buff)
//     long __TM_end (void)
//     void __TM_abort (void)
//     void __TM_named_abort (unsigned char const code)
//     void __TM_resume (void)
//     void __TM_suspend (void)
//     
//     long __TM_is_user_abort (void* const TM_buff)
//     long __TM_is_named_user_abort (void* const TM_buff, unsigned char *code)
//     long __TM_is_illegal (void* const TM_buff)
//     long __TM_is_footprint_exceeded (void* const TM_buff)
//     long __TM_nesting_depth (void* const TM_buff)
//     long __TM_is_nested_too_deep(void* const TM_buff)
//     long __TM_is_conflict(void* const TM_buff)
//     long __TM_is_failure_persistent(void* const TM_buff)
//     long __TM_failure_address(void* const TM_buff)
//     long long __TM_failure_code(void* const TM_buff)

//     #include <htmxlintrin.h>
//     
//     int num_retries = 10;
//     TM_buff_type TM_buff;
//     
//     while (1)
//       {
//         if (__TM_begin (TM_buff) == _HTM_TBEGIN_STARTED)
//           {
//             /* Transaction State Initiated.  */
//             if (is_locked (lock))
//               __TM_abort ();
//             ... transaction code...
//             __TM_end ();
//             break;
//           }
//         else
//           {
//             /* Transaction State Failed.  Use locks if the transaction
//                failure is "persistent" or we've tried too many times.  */
//             if (num_retries-- <= 0
//                 || __TM_is_failure_persistent (TM_buff))
//               {
//                 acquire_lock (lock);
//                 ... non transactional fallback path...
//                 release_lock (lock);
//                 break;
//               }
//           }
//       }

#ifndef SOFTWARE_BARRIER
#define SOFTWARE_BARRIER asm volatile("": : :"memory")
#endif

//#define NO_TXNS
#ifdef NO_TXNS
    #define XBEGIN() _XBEGIN_STARTED; SOFTWARE_BARRIER; //__sync_synchronize();
    #define XEND() { SOFTWARE_BARRIER; } //__sync_synchronize();
    #define XABORT(_status) { SOFTWARE_BARRIER; status = (((_status) << 24) | _XABORT_EXPLICIT); /*__sync_synchronize();*/ goto aborthere; }
    #define XTEST() false
#else
    #define XBEGIN_ARG_T TM_buff_type
    #define XBEGIN(arg) ((__TM_begin((arg))) == _HTM_TBEGIN_STARTED)
    #define XEND() (__TM_end())
    #define XABORT(arg) (__TM_named_abort((arg)))
    #define XSUSPEND() (__TM_suspend())
    #define XRESUME() (__TM_resume())
    #define X_ABORT_GET_STATUS(arg) (__TM_failure_code((arg)))
    #define X_ABORT_COMPRESS_STATUS(status) ((status)&0x1ffff | ((((status)>>31)&0x3)<<17))
    #define X_ABORT_DECOMPRESS_STATUS(cstatus) ((cstatus)&0x1ffff | (((cstatus)>>17)&0x3)<<31)
    #define X_ABORT_STATUS_USERCODE() ((status)&0x7f)
    #define X_ABORT_STATUS_IS_USER(status) (((status)>>31)&0x1)
    #define X_ABORT_STATUS_IS_CAPACITY(status) (((status)>>10)&0x1)
    #define X_ABORT_STATUS_IS_NESTING(status) (((status)>>9)&0x1)
    #define X_ABORT_STATUS_IS_CONFLICT(status) (((status)>>11)&0xf)
    #define X_ABORT_STATUS_IS_RETRY(status) (!(((status)>>7)&0x1))
//    #define X_IS_ABORT_USER(arg, code) (__TM_is_named_user_abort((arg), (code)))
//    #define X_IS_ABORT_CAPACITY(arg) (__TM_is_footprint_exceeded((arg)))
//    #define X_IS_ABORT_NESTING(arg) (__TM_is_nested_too_deep((arg)))
//    #define X_IS_ABORT_CONFLICT(arg) (__TM_is_conflict((arg)))
//    #define X_IS_ABORT_RETRY(arg) (!(__TM_is_failure_persistent((arg))))
//    #define X_IS_ABORT_ILLEGAL(arg) (__TM_is_illegal((arg)))
#endif

#endif
