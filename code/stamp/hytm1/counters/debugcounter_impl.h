/* 
 * File:   debugcounter_impl.h
 * Author: trbot
 *
 * Created on June 21, 2016, 10:23 PM
 */

#ifndef DEBUGCOUNTER_IMPL_H
#define	DEBUGCOUNTER_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "debugcounter.h"
    
void counterAdd(struct debugCounter *c, const int tid, const long long val) {
    c->data[tid*PREFETCH_SIZE_WORDS] += val;
}
void counterInc(struct debugCounter *c, const int tid) {
    counterAdd(c, tid, 1);
}
void counterSet(struct debugCounter *c, const int tid, const long long val) {
    c->data[tid*PREFETCH_SIZE_WORDS] = val;
}
long long counterGet(struct debugCounter *c, const int tid) {
    return c->data[tid*PREFETCH_SIZE_WORDS];
}
long long counterGetTotal(struct debugCounter *c) {
    long long result = 0;
    int tid=0;
    for (;tid<c->NUM_PROCESSES;++tid) {
        result += counterGet(c, tid);
    }
    return result;
}
void counterClear(struct debugCounter *c) {
    int tid=0;
    for (;tid<c->NUM_PROCESSES;++tid) {
        c->data[tid*PREFETCH_SIZE_WORDS] = 0;
    }
}
void counterInit(struct debugCounter *c, const int numProcesses) {
    c->NUM_PROCESSES = numProcesses;
    c->data = (volatile long long *) malloc(numProcesses*PREFETCH_SIZE_BYTES);
    counterClear(c);
}
void counterDestroy(struct debugCounter *c) {
    free((void*) c->data);
}

#ifdef	__cplusplus
}
#endif

#endif	/* DEBUGCOUNTER_IMPL_H */

