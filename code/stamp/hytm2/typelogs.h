/* 
 * File:   typelogs.h
 * Author: trbot
 *
 * Created on April 22, 2016, 1:06 AM
 */

#ifndef TYPELOGS_H
#define	TYPELOGS_H

#include "common.h"
#include "log.h"

class vLock;
class vLockSnapshot;
class Thread;

enum hytm_config {
    INIT_WRSET_NUM_ENTRY = 1024,
    INIT_RDSET_NUM_ENTRY = 8192,
    INIT_LOCAL_NUM_ENTRY = 1024,
};

class TypeLogs {
public:
    Log l;
    Log f;
    long sz;
    
    TypeLogs();
    TypeLogs(Thread* Self, long _sz);

    void clear();

    bool lockAll(Thread* Self);
    
    void releaseAll(Thread* Self);
    
    bool validate(Thread* Self);

    void writeForward();

    void writeReverse();
    
    template <typename T>
    void append(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv);
    
    template <typename T>
    AVPair* findFirst(vLock* Lock);
    
    template <typename T>
    bool contains(vLock* Lock);
};

template <typename T>
Log * getTypedLog(TypeLogs * typelogs);
template <>
Log * getTypedLog<long>(TypeLogs * typelogs);
template <>
Log * getTypedLog<float>(TypeLogs * typelogs);

#endif	/* TYPELOGS_H */

