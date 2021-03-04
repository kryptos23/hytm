/*
 * File:   debugcounters.h
 * Author: trbot
 *
 * Created on January 26, 2015, 2:22 PM
 *
 * This file contains BST-specific debugcounters (and TM-specific ones are in hytm1/counters)
 *
 */

#ifndef DEBUGCOUNTERS_H
#define	DEBUGCOUNTERS_H


#include <string>
#include <sstream>
#include "globals_extern.h"
#include "recordmgr/debugcounter.h"
using namespace std;

typedef long long counter_t;

class threadCounters {
public:
    char padding0[PREFETCH_SIZE_BYTES];
    counter_t updateChange;
    counter_t pathSuccess;
    counter_t pathFail;
    counter_t rebalancingSuccess;
    counter_t rebalancingFail;
    counter_t llxSuccess;
    counter_t llxFail;
    counter_t insertSuccess;
    counter_t insertFail;
    counter_t eraseSuccess;
    counter_t eraseFail;
    counter_t findSuccess;
    counter_t findFail;
    counter_t rqSuccess;
    counter_t rqFail;
    counter_t garbage;
    counter_t keysum;
    counter_t prefillSize;
    char padding1[PREFETCH_SIZE_BYTES];

    void clear() {
        memset(this, 0, sizeof(*this));
    }
    threadCounters() {
        clear();
    }
} __attribute__((aligned(BYTES_IN_CACHE_LINE)));

template <int NUM_PROCESSES>
class debugCounters {
    char padding0[PREFETCH_SIZE_BYTES];
    threadCounters counters[NUM_PROCESSES];
    char padding1[PREFETCH_SIZE_BYTES];
public:
    threadCounters * operator[](int tid) {
        return counters+tid;
    }
    void clear() {
        for (int i=0;i<NUM_PROCESSES;++i) {
            counters[i].clear();
        }
    }
    debugCounters() {
        clear();
    }
};

#endif	/* DEBUGCOUNTERS_H */

