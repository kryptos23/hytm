/* 
 * File:   vlock.h
 * Author: trbot
 *
 * Created on April 22, 2016, 12:58 AM
 */

#ifndef VLOCK_H
#define	VLOCK_H

#include <stdint.h>

#define LOCKBIT 1
class vLockSnapshot {
private:
    uintptr_t lockstate;
public:
    vLockSnapshot();
    vLockSnapshot(uintptr_t _lockstate);
    bool isLocked();
    bool version();
};

class vLock {
    volatile uintptr_t lock; // (Version,LOCKBIT)
private:
    vLock(uintptr_t lockstate);
public:
    vLock();
    vLockSnapshot getSnapshot();
    bool tryAcquire();
    void release();
    // can be invoked only by a hardware transaction
    void htmIncrementVersion();
};

/*
 * Consider 4M alignment for LockTab so we can use large-page support.
 * Alternately, we could mmap() the region with anonymous DZF pages.
 */
#define _TABSZ  (1<< 20)
extern vLock LockTab[_TABSZ];

/*
 * With PS the versioned lock words (the LockTab array) are table stable and
 * references will never fault.  Under PO, however, fetches by a doomed
 * zombie txn can fault if the referent is free()ed and unmapped
 */
#if 0
#define LDLOCK(a)                     LDNF(a)  /* for PO */
#else
#define LDLOCK(a)                     *(a)     /* for PS */
#endif

/*
 * PSLOCK: maps variable address to lock address.
 * For PW the mapping is simply (UNS(addr)+sizeof(int))
 * COLOR attempts to place the lock(metadata) and the data on
 * different D$ indexes.
 */
#define TABMSK (_TABSZ-1)
#define COLOR (128)

/*
 * ILP32 vs LP64.  PSSHIFT == Log2(sizeof(intptr_t)).
 */
#define PSSHIFT ((sizeof(void*) == 4) ? 2 : 3)
#define PSLOCK(a) (LockTab + (((UNS(a)+COLOR) >> PSSHIFT) & TABMSK)) /* PS1M */

#endif	/* VLOCK_H */

