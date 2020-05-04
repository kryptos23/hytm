/*
 * File:   globals_extern.h
 * Author: trbot
 *
 * Created on March 9, 2015, 1:32 PM
 */

#ifndef GLOBALS_EXTERN_H
#define	GLOBALS_EXTERN_H

#include <string>
using namespace std;

#include "recordmgr/machineconstants.h"

extern string PATH_NAMES[];

#define NUMBER_OF_PATHS 3
#define PATH_FAST_HTM 0
#define PATH_SLOW_HTM 1
#define PATH_FALLBACK 2

#include "recordmgr/debugcounter.h"
#include "debugcounters.h" // needed for __rtm_force_inline
#include <atomic>

#ifndef DS_DEBUG
#define DS_DEBUG if(0)
#define DS_DEBUG1 if(0)
#define DS_DEBUG2 if(0)
#endif

#ifdef __unix__
#define POSIX_SYSTEM
#else
#error NOT UNIX SYSTEM
#endif

extern std::atomic_bool ___trace;
#define TRACE_TOGGLE {bool ___t = ___trace; ___trace = !___t;}
#define TRACE_ON {___trace = true;}
#define TRACE DS_DEBUG if(___trace)

extern std::atomic_bool ___validateops;
#define VALIDATEOPS_ON {___validateops = true;}
#define VALIDATEOPS DS_DEBUG if(___validateops)

#include <sstream>
#define COUTATOMIC(coutstr) cout<<coutstr
#define COUTATOMICTID(coutstr) cout<<"tid="<<(tid<10?" ":"")<<tid<<": "<<coutstr

extern double INSERT_FRAC;
extern double DELETE_FRAC;
extern double RQ;
extern int RQSIZE;
extern int MAXKEY;
extern int OPS_PER_THREAD;
extern int MILLIS_TO_RUN;
extern bool PREFILL;
extern int WORK_THREADS;
extern int RQ_THREADS;
extern int TOTAL_THREADS;
extern char * RECLAIM_TYPE;
extern char * ALLOC_TYPE;
extern char * POOL_TYPE;
extern int MAX_FAST_HTM_RETRIES;
extern int MAX_SLOW_HTM_RETRIES;
extern bool PRINT_TREE;

#endif	/* GLOBALS_EXTERN_H */

