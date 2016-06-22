/* 
 * File:   debugcounter.h
 * Author: trbot
 *
 * Created on September 27, 2015, 4:43 PM
 */

#ifndef DEBUGCOUNTER_H
#define	DEBUGCOUNTER_H

#include "../../dsbench/recordmgr/machineconstants.h"

struct debugCounter {
    volatile long long * data; // data[tid*PREFETCH_SIZE_WORDS] = count for thread tid (padded to avoid false sharing)
    int NUM_PROCESSES;
};

void counterAdd(struct debugCounter *c, const int tid, const long long val);
void counterInc(struct debugCounter *c, const int tid);
void counterSet(struct debugCounter *c, const int tid, const long long val);
long long counterGet(struct debugCounter *c, const int tid);
long long counterGetTotal(struct debugCounter *c);
void counterClear(struct debugCounter *c);
void counterInit(struct debugCounter *c, const int numProcesses);
void counterDestroy(struct debugCounter *c);

#endif	/* DEBUGCOUNTER_H */
