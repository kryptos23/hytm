/* 
 * File:   thread.h
 * Author: trbot
 *
 * Created on April 22, 2016, 1:02 AM
 */

#ifndef THREAD_H
#define	THREAD_H

#include "common.h"
#include "tmalloc.h"
#include <setjmp.h>
#include <cassert>

class TypeLogs;

class Thread {
public:
    long UniqID;
    volatile long Retries;
//    int* ROFlag; // not used by stamp
    bool IsRO;
    int isFallback;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    unsigned long long rng;
    unsigned long long xorrng [1];
    tmalloc_t* allocPtr; /* CCM: speculatively allocated */
    tmalloc_t* freePtr; /* CCM: speculatively free'd */
    TypeLogs* rdSet;
    TypeLogs* wrSet;
    TypeLogs* LocalUndo;
    sigjmp_buf* envPtr;
    
    Thread(long id);
    ~Thread();
} __attribute__((aligned(CACHE_LINE_SIZE)));

#endif	/* THREAD_H */

