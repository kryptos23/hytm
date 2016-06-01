/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef RECORDMGR_GLOBALS_H
#define	RECORDMGR_GLOBALS_H

#include "machineconstants.h"

#ifndef DS_DEBUG
#define DS_DEBUG if(0)
#define DS_DEBUG2 if(0)
#endif

// don't touch these options for crash recovery

#define CRASH_RECOVERY_USING_SETJMP
#define SEND_CRASH_RECOVERY_SIGNALS
#define AFTER_NEUTRALIZING_SET_BIT_AND_RETURN_TRUE
#define PERFORM_RESTART_IN_SIGHANDLER
#define SIGHANDLER_IDENTIFY_USING_PTHREAD_GETSPECIFIC

// some useful, data structure agnostic definitions

typedef bool CallbackReturn;
typedef void* CallbackArg;
typedef CallbackReturn (*CallbackType)(CallbackArg);
#ifndef SOFTWARE_BARRIER
#define SOFTWARE_BARRIER asm volatile("": : :"memory")
#endif

// set __trace to true if you want many paths through the code to be traced with cout<<"..." statements
#ifndef TRACE_DEFINED
std::atomic_bool ___trace(0);
std::atomic_bool ___validateops(0);
#define TRACE_DEFINED
#endif

#include <sstream>
#define COUTATOMIC(coutstr) cout<<coutstr
//{ \
    stringstream ss; \
    ss<<coutstr; \
    cout<<ss.str(); \
}
#define COUTATOMICTID(coutstr) cout<<"tid="<<(tid<10?" ":"")<<tid<<": "<<coutstr
// { \
    stringstream ss; \
    ss<<"tid="<<tid<<(tid<10?" ":"")<<": "<<coutstr; \
    cout<<ss.str(); \
}

#endif	/* GLOBALS_H */

