#include <cassert>
#include "thread.h"
#include "platform.h"
#include "typelogs.h"
#include <string.h>
#include <stdlib.h>

volatile long StartTally = 0;
volatile long AbortTally = 0;

__INLINE__ intptr_t AtomicAdd(volatile intptr_t* addr, intptr_t dx) {
    intptr_t v;
    for (v = *addr; CAS(addr, v, v + dx) != v; v = *addr) {
    }
    return (v + dx);
}

Thread::Thread(long id) {
    memset(this, 0, sizeof (*this)); /* Default value for most members */
    UniqID = id;
    rng = id + 1;
    xorrng[0] = rng;

    wrSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    rdSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    LocalUndo = (TypeLogs*) malloc(sizeof(TypeLogs));
    
    *wrSet = TypeLogs(this, INIT_WRSET_NUM_ENTRY);
    *rdSet = TypeLogs(this, INIT_RDSET_NUM_ENTRY);
    *LocalUndo = TypeLogs(this, INIT_LOCAL_NUM_ENTRY);

    allocPtr = tmalloc_alloc(1);
    freePtr = tmalloc_alloc(1);
    assert(allocPtr);
    assert(freePtr);
}

Thread::~Thread() {
    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTally)), Aborts);
    tmalloc_free(allocPtr);
    tmalloc_free(freePtr);
    free(wrSet);
    free(rdSet);
    free(LocalUndo);
}
