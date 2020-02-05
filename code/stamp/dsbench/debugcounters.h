/* 
 * File:   debugcounters.h
 * Author: trbot
 *
 * Created on January 26, 2015, 2:22 PM
 */

#ifndef DEBUGCOUNTERS_H
#define	DEBUGCOUNTERS_H

#include <string>
#include <sstream>
#include "globals_extern.h"
#include "recordmgr/debugcounter.h"
using namespace std;

//// note: max abort code is 31
//#define ABORT_MARKED 1
//#define ABORT_UPDATE_FAILED 2
//#define ABORT_PROCESS_ON_FALLBACK 3
//#define ABORT_SCXRECORD_POINTER_CHANGED6 4
//#define ABORT_SCXRECORD_POINTER_CHANGED5 5
//#define ABORT_SCXRECORD_POINTER_CHANGED4 6
//#define ABORT_SCXRECORD_POINTER_CHANGED3 7
//#define ABORT_SCXRECORD_POINTER_CHANGED2 8
//#define ABORT_SCXRECORD_POINTER_CHANGED1 9
//#define ABORT_SCXRECORD_POINTER_CHANGED0 10
//#define ABORT_SCXRECORD_POINTER_CHANGED_FASTHTM 11
//#define ABORT_NODE_POINTER_CHANGED 12
//#define ABORT_LLX_FAILED 13
//#define ABORT_SCX_FAILED 14
//#define ABORT_LOCK_HELD 15
//#define ABORT_TLE_LOCKED 30
//
//#define MAX_ABORT_STATUS 4096
//
//int getCompressedStatus(const int status) {
//    return (status & 63) | ((status >> 24)<<6);
//}
//
//int getCompressedStatusAutomaticAbortCode(const int compressedStatus) {
//    return compressedStatus & 63;
//}
//
//int getCompressedStatusExplicitAbortCode(const int compressedStatus) {
//    return compressedStatus >> 6;
//}
//
//int getStatusExplicitAbortCode(const int status) {
//    return status >> 24;
//}
//
//string getAutomaticAbortNames(const int compressedStatus) {
//    return "";
//}
//
//string getExplicitAbortName(const int compressedStatus) {
//    return "";
//}
//
//string getAllAbortNames(const int compressedStatus) {
//    stringstream ss;
//    ss<<getExplicitAbortName(compressedStatus)<<getAutomaticAbortNames(compressedStatus);
//    return ss.str();
////    return "";
//}

typedef long long counter_t;

class threadCounters {
public:
//#ifdef RECORD_ABORTS
//    counter_t htmAbort[NUMBER_OF_PATHS*MAX_ABORT_STATUS];
//#else
//    counter_t * htmAbort;
//#endif
//    counter_t htmCommit[NUMBER_OF_PATHS];
    char padding0[PREFETCH_SIZE_BYTES];
    counter_t updateChange;//[NUMBER_OF_PATHS];
    counter_t pathSuccess;//[NUMBER_OF_PATHS];
    counter_t pathFail;//[NUMBER_OF_PATHS];
    counter_t rebalancingSuccess;//[NUMBER_OF_PATHS];
    counter_t rebalancingFail;//[NUMBER_OF_PATHS];
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
    
//#ifdef RECORD_ABORTS
//    void registerHTMAbort(const int status, const int path) {
//        ++htmAbort[path*MAX_ABORT_STATUS+getCompressedStatus(status)];
//    }
//#endif
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
//    threadCounters& operator[](int tid) {
//        return counters[tid];
//    }
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

