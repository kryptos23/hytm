/*
 * File:   globals.h
 * Author: trbot
 *
 * Created on March 9, 2015, 1:32 PM
 */

#ifndef GLOBALS_H
#define	GLOBALS_H

#include <string>
using namespace std;

#include "globals_extern.h"

#ifndef SOFTWARE_BARRIER
#define SOFTWARE_BARRIER asm volatile("": : :"memory")
#endif

// set __trace to true if you want many paths through the code to be traced with cout<<"..." statements
#ifndef TRACE_DEFINED
PAD;
std::atomic_bool ___trace(0);
std::atomic_bool ___validateops(0);
PAD;
#define TRACE_DEFINED
#endif

PAD;
double INSERT_FRAC;
double DELETE_FRAC;
double RQ;
int RQSIZE;
int MAXKEY;
int OPS_PER_THREAD;
int MILLIS_TO_RUN;
bool PREFILL;
int WORK_THREADS;
int RQ_THREADS;
int TOTAL_THREADS;
char * RECLAIM_TYPE;
char * ALLOC_TYPE;
char * POOL_TYPE;
int MAX_FAST_HTM_RETRIES;
int MAX_SLOW_HTM_RETRIES;
bool PRINT_TREE;
string PATH_NAMES[] = {"fast htm", "slow htm", "fallback"};
PAD;

#endif	/* GLOBALS_H */

