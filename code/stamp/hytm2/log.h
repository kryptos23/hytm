/* 
 * File:   log.h
 * Author: trbot
 *
 * Created on April 22, 2016, 1:04 AM
 */

#ifndef LOG_H
#define	LOG_H

class vLock;
class vLockSnapshot;
class AVPair;
class Thread;

class Log {
public:
    AVPair* List;
    AVPair* put; /* Insert position - cursor */
    AVPair* tail; /* CCM: Pointer to last valid entry */
    AVPair* end; /* CCM: Pointer to last entry */
    long ovf; /* Overflow - request to grow */
    long sz;

private:
    // Append at the tail. We want the front of the list, which sees the most
    // traffic, to remains contiguous.
    AVPair* extend();

public:    
    Log();
    // Allocate the primary list as a large chunk so we can guarantee ascending &
    // adjacent addresses through the list. This improves D$ and DTLB behavior.
    Log(long _sz, Thread* Self);

    ~Log();
    
    void clear();

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    void append(volatile T* Addr, T Valu, vLock* _LockFor, vLockSnapshot _rdv);
    
    // Transfer the data in the log to its ultimate location.
    template <typename T>
    void writeReverse();

    // Transfer the data in the log to its ultimate location.
    template <typename T>
    void writeForward();
    
    // Search for first log entry that contains lock.
    AVPair* findFirst(vLock* Lock);
    
    bool contains(vLock* Lock);
    
    // can be invoked only by a transaction on the software path
    bool lockAll(Thread* Self);
    
    // can be invoked only by a transaction on the software path
    void releaseAll(Thread* Self);

    // can be invoked only by a transaction on the software path.
    // writeSet must point to the write-set for this Thread that
    // contains addresses/values of type T.
    template <typename T>
    bool validate(Thread* Self);
};


#endif	/* LOG_H */

