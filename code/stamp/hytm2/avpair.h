/* 
 * File:   avpair.h
 * Author: trbot
 *
 * Created on April 22, 2016, 12:56 AM
 */

#ifndef AVPAIR_H
#define	AVPAIR_H

#include <stdint.h>
#include "vlock.h"

class Thread;

/* Read set and write-set log entry */
class AVPair {
public:
    AVPair* Next;
    AVPair* Prev;
    union {
        volatile long* l;
        volatile float* f;
        volatile intptr_t* p;
    } addr;
    union {
        long l;
#ifdef __LP64__
        float f[2];
#else
        float f[1];
#endif
        intptr_t p;
    } value;
    vLock* LockFor; /* points to the vLock covering Addr */
    vLockSnapshot rdv; /* read-version @ time of 1st read - observed */
    Thread* Owner;
    long Ordinal;
    
    AVPair();
    AVPair(AVPair* _Next, AVPair* _Prev, Thread* _Owner, long _Ordinal);
};

template <typename T>
void assignAV(AVPair* e, volatile T* addr, T value);
template <>
void assignAV<long>(AVPair* e, volatile long* addr, long value);
template <>
void assignAV<float>(AVPair* e, volatile float* addr, float value);

template <typename T>
void replayStore(AVPair* e);
template <>
void replayStore<long>(AVPair* e);
template <>
void replayStore<float>(AVPair* e);

template <typename T>
volatile T* unpackAddr(AVPair* e);
template <>
volatile long* unpackAddr<long>(AVPair* e);
template <>
volatile float* unpackAddr<float>(AVPair* e);

template <typename T>
T unpackValue(AVPair* e);
template <>
long unpackValue<long>(AVPair* e);
template <>
float unpackValue<float>(AVPair* e);

#endif	/* AVPAIR_H */