/* 
 * File:   debugcounters.h
 * Author: trbot
 *
 * Created on June 21, 2016, 9:50 PM
 */

#ifndef DEBUGCOUNTERS_H
#define	DEBUGCOUNTERS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "debugcounter.h"
#include <time.h>

#define NUMBER_OF_PATHS 3
#define PATH_FAST_HTM 0
#define PATH_SLOW_HTM 1
#define PATH_FALLBACK 2

// note: max abort code is 31
#define ABORT_PROCESS_ON_FALLBACK 3
#define ABORT_LOCK_HELD 15
#define ABORT_TLE_LOCKED 30
    
int getCompressedStatus(const int status);

int getCompressedStatusAutomaticAbortCode(const int compressedStatus);

int getCompressedStatusExplicitAbortCode(const int compressedStatus);

int getStatusExplicitAbortCode(const int status);

#define MAX_ABORT_STATUS 4096

struct debugCounters {
    int NUM_PROCESSES;
#ifdef RECORD_ABORTS
    struct debugCounter * htmAbort[NUMBER_OF_PATHS*MAX_ABORT_STATUS]; // one third of these are useless
#else
    struct debugCounter ** htmAbort;
#endif
    struct debugCounter * htmCommit[NUMBER_OF_PATHS];
    struct debugCounter * garbage;
    struct debugCounter * timingTemp; // per process timestamps: 0 if not currently timing, o/w > 0
    struct debugCounter * timingOnFallback; // per process total DURATIONS over execution (scaled down by the probability of timing on a countersProbStartTime call)
};

void registerHTMAbort(struct debugCounters *cs, const int tid, const int status, const int path);
void countersClear(struct debugCounters *cs);
void countersInit(struct debugCounters *cs, const int numProcesses);
void countersDestroy(struct debugCounters *cs);
void countersProbStartTime(struct debugCounters *cs, const int tid, const double randomZeroToOne);
void countersProbEndTime(struct debugCounters *cs, const int tid, struct debugCounter *timingCounter);

#ifdef	__cplusplus
}
#endif

#endif	/* DEBUGCOUNTERS_H */

