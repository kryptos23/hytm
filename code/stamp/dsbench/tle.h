/* 
 * File:   tle.h
 * Author: trbot
 *
 * Created on January 23, 2016, 5:11 PM
 */

#ifndef TLE_H
#define	TLE_H

#include "debugcounters.h"
#include "../hytm1/platform_impl.h"
#include "globals_extern.h"
#include "globals.h"

void ds_acquireLock(volatile int *lock) {
    while (1) {
        if (*lock) {
            PAUSE();
            continue;
        }
        if (__sync_bool_compare_and_swap(lock, false, true)) {
            return;
        }
    }
}

void ds_releaseLock(volatile int *lock) {
    *lock = false;
}

bool readLock(volatile int *lock) {
    return *lock;
}

class TLEScope {
private:
    int full_attempts;
    int volatile * const lock;
    int tid;
    bool locked;
    bool ended;
    debugCounter ** csucc;
    debugCounter ** cfail;
    debugCounter ** chtmabort;
public:
    __rtm_force_inline TLEScope(int volatile * const _lock, const int maxAttempts, const int _tid, debugCounter ** _succ, debugCounter ** _fail, debugCounter ** _htmabort) : lock(_lock), tid(_tid) {
        csucc = _succ;
        cfail = _fail;
        chtmabort = _htmabort;
        full_attempts = 0;
        locked = false;
        ended = false;

        // try transactions
        XBEGIN_ARG_T status;
        while (full_attempts++ < maxAttempts) {
            int attempts = MAX_FAST_HTM_RETRIES;
TXN1: (0);
            if (XBEGIN(status)) {
                if (*lock > 0) XABORT(ABORT_TLE_LOCKED);
                // run the critical section
                goto criticalsection;
            } else {
aborthere:      (0); // aborted
#ifdef RECORD_ABORTS
                //chtmabort[PATH_FAST_HTM*MAX_ABORT_STATUS+getCompressedStatus(status)]->inc(tid);
                //chtmabort->registerHTMAbort(tid, status, PATH_FAST_HTM);
#endif
                //IF_ALWAYS_RETRY_WHEN_BIT_SET if (status & _XABORT_RETRY) { _fail[PATH_FAST_HTM]->inc(tid); goto TXN1; }
                while (*lock) {
                    PAUSE();
                }
            }
        }
        // acquire lock
        while (1) {
            if (*lock) {
                PAUSE();
                continue;
            }
            if (__sync_bool_compare_and_swap(lock, false, true)) {
                locked = true;
                break;
            }
        }
criticalsection: (0);
    }
    ~TLEScope() {
        end();
    }
    void end() {
        if (!ended) {
            ended = true;
            if (locked) {
                *lock = false;
                // locked = false; // unnecessary since this is going out of scope
                csucc[PATH_FALLBACK]->inc(tid);
                cfail[PATH_FAST_HTM]->add(tid, full_attempts-1);
            } else {
                XEND();
                csucc[PATH_FAST_HTM]->inc(tid);
                cfail[PATH_FAST_HTM]->add(tid, full_attempts-1);
            }
        }
    }
    bool isFallback() {
        return locked;
    }
};

#endif	/* TLE_H */

