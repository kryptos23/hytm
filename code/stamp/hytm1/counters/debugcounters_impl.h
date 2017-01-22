/* 
 * File:   debugcounters_impl.h
 * Author: trbot
 *
 * Created on June 21, 2016, 9:50 PM
 */

#ifndef C_DEBUGCOUNTERS_IMPL_H
#define	C_DEBUGCOUNTERS_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "debugcounters.h"
    
#ifdef RECORD_ABORT_ADDRESSES
#define MAX_ABORT_ADDR (1<<20)
int numAbortAddr = 0; // for thread 0
long abortAddr[MAX_ABORT_ADDR];
#endif

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

#define BIT_USER 0
#define BIT_CAPACITY 1
#define BIT_CONFLICT 2
#define BIT_RETRY 3
#define BIT_ILLEGAL 4

void registerHTMAbort(struct c_debugCounters *cs, const int tid, const XBEGIN_ARG_T arg, const int path) {
#ifdef RECORD_ABORTS
    int s = 0;
    if (arg) {
        s = (X_ABORT_STATUS_IS_USER(arg) << BIT_USER)
          | (X_ABORT_STATUS_IS_CAPACITY(arg) << BIT_CAPACITY)
          | (X_ABORT_STATUS_IS_CONFLICT(arg) << BIT_CONFLICT)
          | (X_ABORT_STATUS_IS_RETRY(arg) << BIT_RETRY)
          | (X_ABORT_STATUS_IS_ILLEGAL(arg) << BIT_ILLEGAL);
#ifdef RECORD_ABORT_ADDRESSES
        if (tid == 0) {
            long a = X_ABORT_FAILURE_ADDRESS(arg);
            if (a && numAbortAddr < MAX_ABORT_ADDR) {
                abortAddr[numAbortAddr++] = a;
            }
        }
#endif
    }
    counterInc(cs->htmAbort[path*MAX_ABORT_STATUS+s], tid);
#endif
}

void countersClear(struct c_debugCounters *cs) {
    int j=0;
    for (;j<NUMBER_OF_PATHS;++j) {
#ifdef RECORD_ABORTS
        int i=0;
        for (;i<MAX_ABORT_STATUS;++i) {
            counterClear(cs->htmAbort[j*MAX_ABORT_STATUS+i]);
        }
#endif
        counterClear(cs->htmCommit[j]);
    }
    counterClear(cs->garbage);
    counterClear(cs->timingTemp);
    counterClear(cs->timingOnFallback);
}

void countersInit(struct c_debugCounters *cs, const int numProcesses) {
    cs->NUM_PROCESSES = numProcesses;
    int j=0;
    for (;j<NUMBER_OF_PATHS;++j) {
#ifdef RECORD_ABORTS
        int i=0;
        for (;i<MAX_ABORT_STATUS;++i) {
            cs->htmAbort[j*MAX_ABORT_STATUS+i] = (struct c_debugCounter *) malloc(sizeof(struct c_debugCounter));
            counterInit(cs->htmAbort[j*MAX_ABORT_STATUS+i], cs->NUM_PROCESSES);
        }
#endif
        cs->htmCommit[j] = (struct c_debugCounter *) malloc(sizeof(struct c_debugCounter));
        counterInit(cs->htmCommit[j], cs->NUM_PROCESSES);
    }
    cs->garbage = (struct c_debugCounter *) malloc(sizeof(struct c_debugCounter));
    counterInit(cs->garbage, cs->NUM_PROCESSES);
    cs->timingTemp = (struct c_debugCounter *) malloc(sizeof(struct c_debugCounter));
    counterInit(cs->timingTemp, cs->NUM_PROCESSES);
    cs->timingOnFallback = (struct c_debugCounter *) malloc(sizeof(struct c_debugCounter));
    counterInit(cs->timingOnFallback, cs->NUM_PROCESSES);
}

void countersDestroy(struct c_debugCounters *cs) {
    int j=0;
    for (;j<NUMBER_OF_PATHS;++j) {
#ifdef RECORD_ABORTS
        int i=0;
        for (;i<MAX_ABORT_STATUS;++i) {
            counterDestroy(cs->htmAbort[j*MAX_ABORT_STATUS+i]);
            free(cs->htmAbort[j*MAX_ABORT_STATUS+i]);
        }
#endif
        counterDestroy(cs->htmCommit[j]);
        free(cs->htmCommit[j]);
    }
    counterDestroy(cs->garbage);
    free(cs->garbage);
    counterDestroy(cs->timingTemp);
    free(cs->timingTemp);
    counterDestroy(cs->timingOnFallback);
    free(cs->timingOnFallback);

#ifdef RECORD_ABORT_ADDRESSES
    // output abort addresses
    FILE * pFile;
    pFile = fopen ("abortaddresses.txt","w");
    for (int i=0;i<numAbortAddr;++i) {
        fprintf(pFile, "%lx\n", abortAddr[i]);
    }
    fclose(pFile);
#endif
}

#define TIMING_PROBABILITY_THRESH 0.01

void countersProbStartTime(struct c_debugCounters *cs, const int tid, const double randomZeroToOne) {
    if (randomZeroToOne < TIMING_PROBABILITY_THRESH) {
        timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        long long now = t.tv_sec * 1000000000LL + t.tv_nsec;
        counterSet(cs->timingTemp, tid, now);
    }
}
void countersProbEndTime(struct c_debugCounters *cs, const int tid, struct c_debugCounter *timingCounter) {
    long long start = counterGet(cs->timingTemp, tid);
    if (start > 0) {
        timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        long long now = t.tv_sec * 1000000000LL + t.tv_nsec;
        counterSet(cs->timingTemp, tid, 0);
        counterAdd(timingCounter, tid, (now-start)/*/TIMING_PROBABILITY_THRESH*/);
    }
}

#ifdef	__cplusplus
}
#endif

#endif	/* DEBUGCOUNTERS_IMPL_H */

