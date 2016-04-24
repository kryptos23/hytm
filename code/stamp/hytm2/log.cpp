#include "common.h"
#include "vlock.h"
#include "avpair.h"
#include "thread.h"
#include "log.h"
#include "typelogs.h"
#include <stdlib.h>
#include <string.h>

// Append at the tail. We want the front of the list, which sees the most
// traffic, to remains contiguous.

__INLINE__ AVPair* Log::extendList() {
    ovf++;
    AVPair* e = (AVPair*) malloc(sizeof (*e));
    assert(e);
    tail->Next = e;
    *e = AVPair(NULL, tail, tail->Owner, tail->Ordinal + 1);
    end = e;
    return e;
}

Log::Log() {
}
// Allocate the primary list as a large chunk so we can guarantee ascending &
// adjacent addresses through the list. This improves D$ and DTLB behavior.

Log::Log(long _sz, Thread* Self) {
    List = (AVPair*) malloc((sizeof (*List) * _sz) + CACHE_LINE_SIZE);
    assert(List);
    memset(List, 0, sizeof (*List) * _sz);
    long i;
    AVPair* curr = List;
    put = List;
    end = NULL;
    tail = NULL;
    for (i = 0; i < _sz; i++) {
        AVPair* e = curr++;
        *e = AVPair(curr, tail, Self, i); // note: curr is invalid in the last iteration
        tail = e;
    }
    tail->Next = NULL; // fix invalid next pointer from last iteration
    sz = _sz;
    ovf = 0;
}

Log::~Log() {
    /* Free appended overflow entries first */
    AVPair* e = end;
    if (e != NULL) {
        while (e->Ordinal >= sz) {
            AVPair* tmp = e;
            e = e->Prev;
            free(tmp);
        }
    }
    /* Free contiguous beginning */
    free(List);
}

void Log::clear() {
    put = List;
    tail = NULL;
}

// for undo log: immediate writes -> undo on abort/restart

template <typename T>
__INLINE__ void Log::insert(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv) {
    AVPair* e = put;
    if (e == NULL) {
        e = extendList();
    }
    tail = e;
    put = e->Next;
    assignAV(e, Addr, Valu);
    e->LockFor = _LockFor;
    e->rdv = _rdv;
}

// Transfer the data in the log to its ultimate location.

template <typename T>
__INLINE__ void Log::writeReverse() {
    AVPair* e;
    for (e = tail; e != NULL; e = e->Prev) {
        replayStore<T>(e);
    }
}

// Transfer the data in the log to its ultimate location.

template <typename T>
__INLINE__ void Log::writeForward() {
    AVPair* e;
    AVPair* End = put;
    for (e = List; e != End; e = e->Next) {
        replayStore<T>(e);
    }
}

// Search for first log entry that contains lock.

__INLINE__ AVPair* Log::findFirst(vLock* Lock) {
    AVPair * const End = put;
    for (AVPair* e = List; e != End; e = e->Next) {
        if (e->LockFor == Lock) {
            return e;
        }
    }
    return NULL;
}

__INLINE__ bool Log::contains(vLock* Lock) {
    return findFirst(Lock) != NULL;
}

// can be invoked only by a transaction on the software path

__INLINE__ bool Log::lockAll(Thread* Self) {
    assert(Self->isFallback);

    AVPair * const stop = put;
    for (AVPair* curr = List; curr != stop; curr = curr->Next) {
        // if we fail to acquire a lock
        if (!curr->LockFor->tryAcquire()) {
            // unlock all previous entries (since we locked them) and return false
            for (AVPair* toUnlock = List; toUnlock != curr; toUnlock = toUnlock->Next) {
                toUnlock->LockFor->release();
            }
            return false;
        }
    }
    return true;
}

// can be invoked only by a transaction on the software path

__INLINE__ void Log::releaseAll(Thread* Self) {
    assert(Self->isFallback);

    AVPair * const stop = put;
    for (AVPair* curr = List; curr != stop; curr = curr->Next) {
        curr->LockFor->release();
    }
}

// can be invoked only by a transaction on the software path.
// writeSet must point to the write-set for this Thread that
// contains addresses/values of type T.

template <typename T>
__INLINE__ bool Log::validate(Thread* Self) {
    assert(Self->isFallback);

    AVPair * const stop = put;
    for (AVPair* curr = List; curr != stop; curr = curr->Next) {
        T* Addr = unpackAddr<T>(curr);
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked() || locksnap.version() != curr->rdv.version()) {
            // if write set does not contain Addr
            if (!Self->wrSet->contains<T>(lock)) { // TODO: improve contains w/hashing or bloom filter
                return false;
            }
        }
    }
    return true;
}
