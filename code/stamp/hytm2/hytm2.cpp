/**
 * Code for HyTM is loosely based on the code for TL2
 * (in particular, the data structures)
 * 
 * This is an implementation of Algorithm 1 from the paper.
 * 
 * [ note: we cannot distribute this without inserting the appropriate
 *         copyright notices as required by TL2 and STAMP ]
 * 
 * Authors: Trevor Brown (tabrown@cs.toronto.edu) and Srivatsan Ravi
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "platform.h"
#include "hytm2.h"
#include "stm.h"
#include "tmalloc.h"
#include "util.h"
#include "../murmurhash/MurmurHash3.h"
#include <iostream>
#include <execinfo.h>
using namespace std;

// todo: remove locks hashlist, and add owner field of lock instead
// todo: optimize starting a txn by only doing initialization of d.s. for s/w txn just before the first s/w txn!

// just for debugging
volatile int globallock = 0;

void printStackTrace() {

  void *trace[16];
  char **messages = (char **)NULL;
  int i, trace_size = 0;

  trace_size = backtrace(trace, 16);
  messages = backtrace_symbols(trace, trace_size);
  /* skip first stack frame (points here) */
  printf("  [bt] Execution path:\n");
  for (i=1; i<trace_size; ++i)
  {
    printf("    [bt] #%d %s\n", i, messages[i]);

    /* find first occurence of '(' or ' ' in message[i] and assume
     * everything before that is the file name. (Don't go beyond 0 though
     * (string terminator)*/
    int p = 0; //size_t p = 0;
    while(messages[i][p] != '(' && messages[i][p] != ' '
            && messages[i][p] != 0)
        ++p;
    
    char syscom[256];
    sprintf(syscom,"echo \"    `addr2line %p -e %.*s`\"", trace[i], p, messages[i]);
        //last parameter is the file name of the symbol
    if (system(syscom) < 0) {
        printf("ERROR: could not run necessary command to build stack trace\n");
        exit(-1);
    };
  }

  exit(-1);
}

void acquireLock(volatile int *lock) {
    while (1) {
        if (*lock) {
            __asm__ __volatile__("pause;");
            continue;
        }
        if (__sync_bool_compare_and_swap(lock, 0, 1)) {
            return;
        }
    }
}

void releaseLock(volatile int *lock) {
    *lock = 0;
}













/**
 * 
 * TRY-LOCK IMPLEMENTATION AND LOCK TABLE
 * 
 */

class Thread;

#define LOCKBIT 1
class vLockSnapshot {
public:
//private:
    uint64_t lockstate;
public:
    vLockSnapshot() {}
    vLockSnapshot(uint64_t _lockstate) {
        lockstate = _lockstate;
    }
    __INLINE__ bool isLocked() const {
        return lockstate & LOCKBIT;
    }
    __INLINE__ uint64_t version() const {
//        cout<<"LOCKSTATE="<<lockstate<<" ~LOCKBIT="<<(~LOCKBIT)<<" VERSION="<<(lockstate & (~LOCKBIT))<<endl;
        return lockstate & (~LOCKBIT);
    }
    friend std::ostream& operator<<(std::ostream &out, const vLockSnapshot &obj);
};

class vLock {
    volatile uint64_t lock; // (Version,LOCKBIT)
private:
    vLock(uint64_t lockstate) {
        lock = lockstate;
    }
public:
    vLock() {
        lock = 0;
    }
    __INLINE__ vLockSnapshot getSnapshot() const {
        return vLockSnapshot(lock);
    }
    __INLINE__ bool tryAcquire() {
        uint64_t val = lock & (~LOCKBIT);
        return __sync_bool_compare_and_swap(&lock, val, val+1);
    }
    __INLINE__ bool tryAcquire(vLockSnapshot& oldval) {
        return __sync_bool_compare_and_swap(&lock, oldval.version(), oldval.version()+1);
    }
    __INLINE__ void release() {
        ++lock;
    }
    // can be invoked only by a hardware transaction
    __INLINE__ void htmIncrementVersion() {
        lock += 2;
    }
};

std::ostream& operator<<(std::ostream& out, const vLockSnapshot& obj) {
    return out<<"<ver="<<obj.version()
                <<",locked="<<obj.isLocked()<<">"
                ;//<<"> (raw="<<obj.lockstate<<")";
}

std::ostream& operator<<(std::ostream& out, const vLock& obj) {
    return out<<obj.getSnapshot()<<"@"<<(&obj);
}

/*
 * Consider 4M alignment for LockTab so we can use large-page support.
 * Alternately, we could mmap() the region with anonymous DZF pages.
 */
#define _TABSZ  (1<< 20)
static vLock LockTab[_TABSZ];

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










/**
 * 
 * THREAD CLASS
 *
 */

class TypeLogs;

class Thread {
public:
    long UniqID;
    volatile long Retries;
//    int* ROFlag; // not used by stamp
    int IsRO;
    int isFallback;
//    long Starts; // how many times the user called TxBegin
    long AbortsHW; // # of times hw txns aborted
    long AbortsSW; // # of times sw txns aborted
    long CommitsHW;
    long CommitsSW;
    unsigned long long rng;
    unsigned long long xorrng [1];
    tmalloc_t* allocPtr;    /* CCM: speculatively allocated */
    tmalloc_t* freePtr;     /* CCM: speculatively free'd */
    TypeLogs* rdSet;
    TypeLogs* wrSet;
    TypeLogs* LocalUndo;
    sigjmp_buf* envPtr;
    
    Thread(long id);
    void destroy();
    void compileTimeAsserts() {
        CTASSERT(sizeof(*this) == sizeof(Thread_void));
    }
};// __attribute__((aligned(CACHE_LINE_SIZE)));











/**
 * 
 * LOG IMPLEMENTATION
 * 
 */

/* list element (serves as an entry in a read/write set) */
class AVPair {
public:
    uintptr_t keyToHash; // key that is hashed when this is present in a hash table
    AVPair* Next;
    AVPair* Prev;
    intptr_t addr;
    union {
        long l;
#ifdef __LP64__
        float f[2];
#else
        float f[1];
#endif
        intptr_t p;
    } value;
    vLock* LockFor;     /* points to the vLock covering Addr */
    vLockSnapshot rdv;  /* read-version @ time of 1st read - observed */
    Thread* Owner;
    long Ordinal;
    
    AVPair() {}
    AVPair(AVPair* _Next, AVPair* _Prev, Thread* _Owner, long _Ordinal)
        : keyToHash(0), Next(_Next), Prev(_Prev), addr(0), LockFor(0), rdv(0), Owner(_Owner), Ordinal(_Ordinal) {
        value.l = 0;
    }
    
    void validateInvariants() {
//        if (Next || Prev) {
//            if (!LockFor || !Owner || !addr) { // THIS CASE IS WRONG: it's not a problem!
//                ERROR("AVPair invalid; fields are zero when they shouldn't be "<<debug(this));
//            }
//        } else {
//            if (LockFor || Owner || addr || value.l || rdv.lockstate || Ordinal) {
//                ERROR("AVPair invalid; next and prev are null, but some fields are initialized"<<debug(this));
//            }
//        }
    }
};

std::ostream& operator<<(std::ostream& out, const AVPair& obj) {
    return out<<"[addr="<<(void*) obj.addr
            <<" val="<<obj.value.l
            <<" lock@"<<obj.LockFor
            <<" prev="<<obj.Prev
            <<" next="<<obj.Next
            <<" ord="<<obj.Ordinal
            //<<" rdv="<<obj.rdv<<"@"<<(uintptr_t)(long*)&obj
            <<"]@"<<&obj;
}

template <typename T>
inline void assignValue(AVPair* e, T value);
template <>
inline void assignValue<long>(AVPair* e, long value) {
    e->value.l = value;
}
template <>
inline void assignValue<float>(AVPair* e, float value) {
    e->value.f[0] = value;
}

template <typename T>
inline void replayStore(AVPair* e);
template <>
inline void replayStore<long>(AVPair* e) {
    *((long*)e->addr) = e->value.l;
}
template <>
inline void replayStore<float>(AVPair* e) {
    *((float*)e->addr) = e->value.f[0];
}

template <typename T>
inline T unpackValue(AVPair* e);
template <>
inline long unpackValue<long>(AVPair* e) {
    return e->value.l;
}
template <>
inline float unpackValue<float>(AVPair* e) {
    return e->value.f[0];
}

class Log;

template <typename T>
inline Log * getTypedLog(TypeLogs * typelogs);

enum hytm_config {
    INIT_WRSET_NUM_ENTRY = 1024,
    INIT_RDSET_NUM_ENTRY = 8192,
    INIT_LOCAL_NUM_ENTRY = 1024,
};

class HashTable {
public:
    uint32_t seed;  // seed for hash function
//    uintptr_t* keys;
    AVPair** data;
    long sz;        // number of elements in the hash table
    long cap;       // capacity of the hash table
private:
    void validateInvariants() {
        // hash table capacity is a power of 2
        long htc = cap;
        while (htc > 0) {
            if ((htc & 1) && (htc != 1)) {
                ERROR(debug(cap)<<" is not a power of 2");
            }
            htc /= 2;
        }
        // htabcap >= 2*htabsz
        if (requiresExpansion()) {
            ERROR("hash table capacity too small: "<<debug(cap)<<" "<<debug(sz));
        }
    #ifdef LONG_VALIDATION
        // htabsz = size of hash table
        long _htabsz = 0;
        for (int i=0;i<cap;++i) {
            if (data[i]) {
                ++_htabsz; // # non-null entries of htab
            }
        }
        if (sz != _htabsz) {
            ERROR("hash table size incorrect: "<<debug(sz)<<" "<<debug(_htabsz));
        }
    #endif
    }
    
public:
    __INLINE__ void init(const uint32_t _seed, const long _sz) {
        // assert: _sz is a power of 2!
        DEBUG3 aout("hash table "<<this<<" init");
        sz = 0;
        seed = _seed;
        cap = 2 * _sz;
//        keys = (uintptr_t*) malloc(sizeof(uintptr_t) * cap);
//        memset(keys, 0, sizeof(uintptr_t) * cap);
        data = (AVPair**) malloc(sizeof(AVPair*) * cap);
        memset(data, 0, sizeof(AVPair*) * cap);
        VALIDATE_INV(this);
    }
    
    __INLINE__ void destroy() {
        DEBUG3 aout("hash table "<<this<<" destroy");
        free(data);
//        free(keys);
    }
    
    __INLINE__ int hash(uintptr_t p) {
        // assert: htabcap is a power of 2
        return (int) MurmurHash3_x86_32_uintptr(p, seed) & (cap-1);
    }
    
    __INLINE__ int findIx(uintptr_t p) {
        int ix = hash(p);
        while (data[ix]) {
//            if (keys[ix] == p) {
            if (data[ix]->keyToHash == p) {
                return ix;
            }
            ix = (ix + 1) & (cap-1);
        }
        return -1;
    }
    
    __INLINE__ AVPair* find(uintptr_t p) {
        int ix = findIx(p);
        if (ix < 0) return NULL;
        return data[ix];
    }
    
    // assumes there is space for e, and e is not in the hash table
    __INLINE__ void insertFresh(uintptr_t p, AVPair* e) {
        DEBUG3 aout("hash table "<<this<<" insertFresh("<<debug(p)<<", "<<debug(e)<<")");
        assert(p == e->keyToHash);
        VALIDATE_INV(this);
        int ix = hash(p);
        while (data[ix]) { // assumes hash table does NOT contain e
            ix = (ix + 1) & (cap-1);
        }
        data[ix] = e;
        ++sz;
        VALIDATE_INV(this);
    }
    
    __INLINE__ int requiresExpansion() {
        return 2*sz > cap;
    }
   
private:
    // expand table by a factor of 2
    __INLINE__ void expandAndClear() {
//        uintptr_t* oldkeys = keys;
        AVPair** olddata = data;
        init(seed, cap); // note: cap will be doubled by init())
        free(olddata);
//        free(keys);
    }
    
public:
    __INLINE__ void expandAndRehashFromList(AVPair* head, AVPair* stop) {
        DEBUG3 aout("hash table "<<this<<" expandAndRehashFromList");
        VALIDATE_INV(this);
        expandAndClear();
        for (AVPair* e = head; e != stop; e = e->Next) {
            insertFresh(e->keyToHash, e);
        }
        VALIDATE_INV(this);
    }
    
    void validateContainsAllAndSameSize(AVPair* head, AVPair* stop, const int listsz) {
//        if (head == NULL && listsz == 0 && sz == 0) {
//            return;
//        }
//        DEBUG3 aout("hash table "<<this<<" validateContainsAllAndSameSize");
        // each element of list appears in hash table
        for (AVPair* e = head; e != stop; e = e->Next) {
//            aout("  "<<debug(e));
//            if (e) {aout("  "<<debug(*e));}
            
            if (find(e->keyToHash) != e) {
                ERROR("element "<<debug(*e)<<" of list was not in "<<(e->keyToHash == (uintptr_t) e->LockFor ? "lock" : "addr")<<" hash table");
            }
        }
        if (listsz != sz) {
            ERROR("list and hash table sizes differ: "<<debug(listsz)<<" "<<debug(sz));
        }
    }
};

// TODO: try shrinking the initial list sizes to a small manageable amount, then printing the list up to capacity...
// TODO: remove lock hashlist, and replace it with an owner field inside a lock
//   (a void* that is initially NULL, and is set to NULL on every release,
//   and set to Self on every acquire (NOT atomic with acquisition;
//   just need it to manage the question "do I hold the lock?"))

class List {
public:
    // linked list (for iteration)
    AVPair* head;
    //char padding[CACHE_LINE_SIZE];
    AVPair* put;    /* Insert position - cursor */
    AVPair* tail;   /* CCM: Pointer to last valid entry */
    AVPair* end;    /* CCM: Pointer to last entry */
    long ovf;       /* Overflow - request to grow */
    long initcap;
    long currsz;
    
private:
    __INLINE__ AVPair* extendList() {
        VALIDATE_INV(this);
        // Append at the tail. We want the front of the list,
        // which sees the most traffic, to remains contiguous.
        ovf++;
        AVPair* e = (AVPair*) malloc(sizeof(*e));
        assert(e);
        tail->Next = e;
        *e = AVPair(NULL, tail, tail->Owner, tail->Ordinal+1);
        end = e;
        VALIDATE_INV(this);
        return e;
    }
    
    void validateInvariants() {
        // currsz == size of list
        long _currsz = 0;
        AVPair* stop = put;
        for (AVPair* e = head; e != stop; e = e->Next) {
            VALIDATE_INV(e);
            ++_currsz;
        }
        if (currsz != _currsz) {
            ERROR("list size incorrect: "<<debug(currsz)<<" "<<debug(_currsz));
        }

        // capacity is correct and next fields are not too far
        long _currcap = 0;
        for (AVPair* e = head; e; e = e->Next) {
            VALIDATE_INV(e);
            if (e->Next > head+initcap && ovf == 0) {
                ERROR("list has AVPair with a next field that jumps off the end of the AVPair array, but ovf is 0; "<<debug(*e));
            }
            if (e->Next && e->Next != e+1) {
                ERROR("list has incorrect distance between AVPairs; "<<debug(e)<<" "<<debug(e->Next));
            }
            ++_currcap;
        }
        if (_currcap != initcap) {
            ERROR("list capacity incorrect: "<<debug(initcap)<<" "<<debug(_currcap));
        }
    }
public:
    void init(Thread* Self, long _initcap) {
        DEBUG3 aout("list "<<this<<" init");
        // assert: _sz is a power of 2

        // Allocate the primary list as a large chunk so we can guarantee ascending &
        // adjacent addresses through the list. This improves D$ and DTLB behavior.
        head = (AVPair*) malloc((sizeof (AVPair) * _initcap) + CACHE_LINE_SIZE);
        assert(head);
        memset(head, 0, sizeof(AVPair) * _initcap);
        AVPair* curr = head;
        put = head;
        end = NULL;
        tail = NULL;
        for (int i = 0; i < _initcap; i++) {
            AVPair* e = curr++;
            *e = AVPair(curr, tail, Self, i); // note: curr is invalid in the last iteration
            tail = e;
        }
        tail->Next = NULL; // fix invalid next pointer from last iteration
        // TODO: validate list has correct forward + back pointers, and has _sz AVPairs, the last of which points to NULL
        initcap = _initcap;
        ovf = 0;
        currsz = 0;
        VALIDATE_INV(this);
    }
    
    void destroy() {
        DEBUG3 aout("list "<<this<<" destroy");
        /* Free appended overflow entries first */
        AVPair* e = end;
        if (e != NULL) {
            while (e->Ordinal >= initcap) {
                AVPair* tmp = e;
                e = e->Prev;
                free(tmp);
            }
        }
        /* Free contiguous beginning */
        free(head);
    }
    
    void clear() {
        DEBUG3 aout("list "<<this<<" clear");
        VALIDATE_INV(this);
        put = head;
        tail = NULL;
        currsz = 0;
        VALIDATE_INV(this);
    }

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    __INLINE__ AVPair* insertFresh(uintptr_t keyToHash, volatile T* addr, T value, vLock* _LockFor, vLockSnapshot _rdv) {
        DEBUG3 aout("list "<<this<<" insertFresh("<<debug(keyToHash)<<","<<debug(addr)<<","<<debug(value)<<","<<debug(_LockFor)<<","<<debug(_rdv)<<")");
        // assert: addr is NOT in the list
        VALIDATE_INV(this);
        AVPair* e = put;
        if (e == NULL) e = extendList();
        tail = e;
        put = e->Next;
        e->addr = (intptr_t) addr;
        assignValue<T>(e, value);
        e->LockFor = _LockFor;
        e->rdv = _rdv;
        e->keyToHash = keyToHash;
        ++currsz;
        VALIDATE_INV(this);
        return e;
    }
    
    void validateContainsAllAndSameSize(HashTable* tab) {
        if (currsz != tab->sz) {
            ERROR("hash table "<<debug(tab->sz)<<" has different size from list "<<debug(currsz));
        }
        AVPair* stop = put;
        // each element of hash table appears in list
        for (int i=0;i<tab->cap;++i) {
            AVPair* elt = tab->data[i];
            if (elt) {
                // element in hash table; is it in the list?
                bool found = false;
                for (AVPair* e = head; e != stop; e = e->Next) {
                    if (e == elt) {
                        found = true;
                    }
                }
                if (!found) {
                    ERROR("element "<<debug(*elt)<<" of hash table was not in list");
                }
            }
        }
    }
};

std::ostream& operator<<(std::ostream& out, const List& obj) {
    AVPair* stop = obj.put;
    for (AVPair* curr = obj.head; curr != stop; curr = curr->Next) {
        out<<*curr<<(curr->Next == stop ? "" : " ");
    }
    return out;
}



class HashList {
public:
    List list;
private:
    HashTable tab;
    
    void validateInvariants();
public:
    __INLINE__ void init(Thread* Self, long _sz, const uint32_t seed) {
        DEBUG3 aout("hashlist "<<this<<" init");
        // assert: _sz is a power of 2
        list.init(Self, _sz);
        tab.init(seed, _sz);
        VALIDATE_INV(this);
    }

    __INLINE__ AVPair* getListHead() {
        return list.head;
    }
    
    void destroy() {
        DEBUG3 aout("hashlist "<<this<<" destroy");
        list.destroy();
        tab.destroy();
    }
    
    void clear() {
        DEBUG3 aout("hashlist "<<this<<" clear");
        VALIDATE_INV(this);
        
        // clear hash table by nulling out slots for the AVPairs in the list
        AVPair* stop = list.put;
        for (AVPair* e = list.head; e != stop; e = e->Next) {
            int ix = tab.findIx(e->keyToHash);
            if (ix < 0) continue;
            //htab[ix] = NULL;
            // NOTE: when using probing as the collision resolution strategy,
            //       it is insufficient to NULL out only htab[ix].
            //       this is because other inserted elements may be placed
            //       in other cells simply because htab[ix] was full,
            //       and they may no longer be found if htab[ix] is null.
            //       thus, we also want to null out everything else that
            //       hashes to ix but is placed elsewhere due to probing.
            //       since we use linear probing, it suffices to null out
            //       ix and everything that follows it, up until the first
            //       null entry.
            // warning: this may not be enough... we may also need to go
            //       backwards until we reach a preceding null entry.
            //       (actually, i think it should be enough...)
            for (int i=0;i<tab.cap;++i) {
                int probeix = (ix+i) & (tab.cap-1);
                if (tab.data[probeix]) {
                    tab.data[probeix] = NULL;
                } else {
                    break;
                }
            }
        }
        tab.sz = 0;
        
        // clear list
        list.clear();
        VALIDATE_INV(this);
    }

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    __INLINE__ void insert(uintptr_t keyToHash, volatile T* addr, T value, vLock* _LockFor, vLockSnapshot _rdv) {
        DEBUG3 aout("hashlist "<<this<<" insert("<<debug(keyToHash)<<","<<debug(addr)<<","<<debug(value)<<","<<debug(_LockFor)<<","<<debug(_rdv)<<")");
        VALIDATE_INV(this);
        AVPair* e = tab.find(keyToHash);
        
        // this address is NOT in the log
        if (e == NULL) {
            e = list.insertFresh<T>(keyToHash, addr, value, _LockFor, _rdv);
            DEBUG2 aout("thread "<<"?"/*Self->UniqID*/<<" inserted "<<debug(*e)<<" into "<<(keyToHash == (uintptr_t) _LockFor ? "lock" : "addr")<<" list");
            
            // add it to the hash table
            // (first, expand the table if necessary)
            if (tab.requiresExpansion()) {
                // note: expandAndRehashFromList inserts everything in the list
                // into the hash table, which includes e.
                // so we shouldn't insert e ourselves.
                tab.expandAndRehashFromList(list.head, list.put);
            } else {
                tab.insertFresh(keyToHash, e);
                DEBUG2 aout("thread "<<"?"/*Self->UniqID*/<<" inserted "<<debug(*e)<<" into "<<(keyToHash == (uintptr_t) _LockFor ? "lock" : "addr")<<" hash table");
            }
            
        // this address is already in the log
        } else {
            // note: technically, this is unnecessary work if keyToHash is a pointer to Lock
            // update the value associated with Addr
            DEBUG2 aout("thread "<<"?"/*Self->UniqID*/<<" updates value for "<<(keyToHash == (uintptr_t) _LockFor ? "lock" : "addr")<<" list element "<<debug(*e)<<" to new value "<<value);
            assignValue(e, value);
            // note: we keep the old position in the list,
            // and the old lock version number (and everything else).
        }
        // for validation: check new element is in list and hash table (and sizes increased)?
        VALIDATE {
            // list
            AVPair* stop = list.put;
            bool found = false;
            for (AVPair* _e = list.head; _e != stop; _e = _e->Next) {
                if (e == _e) {
                    found = true;
                }
            }
            if (!found) {
                ERROR("after insertion of "<<debug(e)<<" it is not in the list");
            }
            // hash table
            if (tab.find(keyToHash) != e) {
                ERROR("after insertion of "<<debug(e)<<" it is not in the hash table");
            }
        }
        VALIDATE_INV(this); // check all data structure invariants hold
    }
    
    __INLINE__ AVPair* find(uintptr_t keyToHash) {
        return tab.find(keyToHash);
    }
    
    __INLINE__ bool contains(uintptr_t keyToHash) {
        return tab.findIx(keyToHash) != -1;
    }
};

void HashList::validateInvariants() {
    // each element of list appears in hash table
    tab.validateContainsAllAndSameSize(list.head, list.put, list.currsz);
#ifdef LONG_VALIDATION
    // each element of hash table appears in list
    list.validateContainsAllAndSameSize(&tab);
    
    // check no list dupes
    AVPair* stop = list.put;
    for (AVPair* e1 = list.head; e1 != stop; e1 = e1->Next) {
        for (AVPair* e2 = e1->Next; e2 != stop; e2 = e2->Next) {
            if (e1 == e2) {
                ERROR("element "<<debug(*e1)<<" in the list twice; second instance: "<<debug(*e2));
            }
            if (e1->addr == e2->addr) {
                ERROR("two elements "<<debug(*e1)<<" and "<<debug(*e2)<<" with the same address");
            }
        }
    }
#endif
}

std::ostream& operator<<(std::ostream& out, const HashList& obj) {
    return out<<obj.list;
}

class Log {
public:
    HashList addresses;
//    HashList locks;
    
public:
    void init(Thread* Self, long _sz, const uint32_t seed) {
        DEBUG3 aout("log "<<this<<" init");
        // assert: _sz is a power of 2
        addresses.init(Self, _sz, seed);
//        locks.init(Self, _sz, seed);
    }

    void destroy() {
        DEBUG3 aout("log "<<this<<" destroy");
        addresses.destroy();
//        locks.destroy();
    }
    
    void clear() {
        DEBUG3 aout("log "<<this<<" clear");
        addresses.clear();
//        locks.clear();
    }

    // for undo log: immediate writes -> undo on abort/restart
    template <typename T>
    __INLINE__ void insert(volatile T* addr, T value, vLock* _LockFor, vLockSnapshot _rdv) {
        DEBUG3 aout("log "<<this<<" insert("<<debug(addr)<<","<<debug(value)<<","<<debug(_LockFor)<<","<<debug(_rdv)<<")");
        addresses.insert((uintptr_t) addr, addr, value, _LockFor, _rdv);
//        locks.insert((uintptr_t) _LockFor, addr, value, _LockFor, _rdv);
    }
    
    // Transfer the data in the log to its ultimate location.
    template <typename T>
    __INLINE__ void writeReverse() {
        DEBUG3 aout("log "<<this<<" writeReverse");
        for (AVPair* e = addresses.list.tail; e != NULL; e = e->Prev) {
            replayStore<T>(e);
        }
    }

    // Transfer the data in the log to its ultimate location.
    template <typename T>
    __INLINE__ void writeForward() {
        DEBUG3 aout("log "<<this<<" writeForward");
        AVPair* stop = addresses.list.put;
        for (AVPair* e = addresses.list.head; e != stop; e = e->Next) {
            replayStore<T>(e);
        }
    }
    
//    __INLINE__ AVPair* findLock(vLock* lock) {
//        return locks.find((uintptr_t) lock);
//    }
    
    __INLINE__ AVPair* find(uintptr_t addr) {
        return addresses.find(addr);
    }
    
//    __INLINE__ bool containsLock(vLock* lock) {
//        return locks.contains((uintptr_t) lock);
//    }
    
    __INLINE__ bool contains(uintptr_t addr) {
        return addresses.contains(addr);
    }
    
};

std::ostream& operator<<(std::ostream& out, const Log& obj) {
    return out<<obj.addresses; //out<<"\n  addresses: "<<obj.addresses<<"\n  locks: "<<obj.locks;
}

class TypeLogs {
public:
    Log l;
    Log f;
    HashList locks;
    // need lock hashlist HERE so we have only one list of locks for both log types!!! this way, we won't be blocked by trying to acquire the same lock in f and l...
    long sz;
    
    void init(Thread* Self, long _sz) {
        DEBUG3 aout("typelogs "<<this<<" init");
        const uint32_t hashFuncSeed = 0xc4d1ca82; // random number drawn from atmospheric noise (see random.org)
        sz = _sz;
        l.init(Self, _sz, hashFuncSeed);
        f.init(Self, _sz, hashFuncSeed);
        locks.init(Self, _sz, hashFuncSeed);
    }
    
    void destroy() {
        DEBUG3 aout("typelogs "<<this<<" destroy");
        l.destroy();
        f.destroy();
        locks.destroy();
    }

    __INLINE__ void clear() {
        DEBUG3 aout("typelogs "<<this<<" clear");
        l.clear();
        f.clear();
        locks.clear();
    }
    
    __INLINE__ void writeForward() {
        DEBUG3 aout("typelogs "<<this<<" writeForward");
        l.writeForward<long>();
        f.writeForward<float>();
    }

    __INLINE__ void writeReverse() {
        DEBUG3 aout("typelogs "<<this<<" writeReverse");
        l.writeReverse<long>();
        f.writeReverse<float>();
    }
    
    template <typename T>
    __INLINE__ void insert(volatile T* addr, T value, vLock* _LockFor, vLockSnapshot _rdv) {
        DEBUG3 aout("typelogs "<<this<<" insert("<<debug(addr)<<","<<debug(value)<<","<<debug(_LockFor)<<","<<debug(_rdv)<<")");
        Log *_log = getTypedLog<T>(this);
        _log->insert(addr, value, _LockFor, _rdv);
        locks.insert((uintptr_t) _LockFor, addr, value, _LockFor, _rdv);
    }
    
    template <typename T>
    __INLINE__ AVPair* findAddr(uintptr_t addr) {
        Log *_log = getTypedLog<T>(this);
        return _log->find(addr);
    }
    
    template <typename T>
    __INLINE__ bool containsAddr(uintptr_t addr) {
        Log *_log = getTypedLog<T>(this);
        return _log->contains(addr);
    }
    
//    template <typename T>
    __INLINE__ AVPair* findLock(vLock* lock) {
        return locks.find((uintptr_t) lock);
    }
    
//    template <typename T>
    __INLINE__ bool containsLock(vLock* lock) {
        return locks.contains((uintptr_t) lock);
    }
//    
//    template <typename T>
//    __INLINE__ vLockSnapshot* getLockSnapshot(AVPair* e) {
//        AVPair* av = find<T>(e->LockFor);
//        if (av) return &av->rdv;
//        return NULL;
//    }
};

std::ostream& operator<<(std::ostream& out, const TypeLogs& obj) {
    return out<<"longs=["<<obj.l<<"]\nfloats=["<<obj.f<<"]";
}

template <>
__INLINE__ Log * getTypedLog<long>(TypeLogs * typelogs) {
    return &typelogs->l;
}
template <>
__INLINE__ Log * getTypedLog<float>(TypeLogs * typelogs) {
    return &typelogs->f;
}

__INLINE__ vLockSnapshot* minLockSnapshot(vLockSnapshot* a, vLockSnapshot* b) {
    if (a && b) {
        return (a->version() < b->version()) ? a : b;
    } else {
        return a ? a : b; // note: might return NULL
    }
}

__INLINE__ vLockSnapshot* getMinLockSnap(AVPair* a, AVPair* b) {
    if (a && b) {
        return minLockSnapshot(&a->rdv, &b->rdv);
    } else {
        return a ? &a->rdv : (b ? &b->rdv : NULL);
    }
}

// can be invoked only by a transaction on the software path
//template <typename T>
/*__INLINE__*/ bool lockAll(Thread* Self, List* lockAVPairs) {
    DEBUG3 aout("lockAll "<<*lockAVPairs);
    assert(Self->isFallback);

    AVPair* const stop = lockAVPairs->put;
    for (AVPair* curr = lockAVPairs->head; curr != stop; curr = curr->Next) {
        // determine when we first encountered curr->addr in txload or txstore.
        // curr->rdv contains when we first encountered it in a txSTORE,
        // so we also need to compare it with any rdv stored in the READ-set.
        AVPair* readLogEntry = Self->rdSet->findLock(curr->LockFor);
        vLockSnapshot *encounterTime = getMinLockSnap(curr, readLogEntry);
        assert(encounterTime);
//        if (encounterTime->version() != readLogEntry->rdv.version()) {
//            fprintf(stderr, "WARNING: read encounter time was not taken as the minimum\n");
//        }
        
//        vLockSnapshot currsnap = curr->rdv;
        DEBUG2 aout("thread "<<Self->UniqID<<" trying to acquire lock "
                    <<*curr->LockFor
                    <<" with old-ver "<<encounterTime->version()
                    //<<" (and curr-ver "<<currsnap.version()<<" raw="<<currsnap.lockstate<<" raw&(~1)="<<(currsnap.lockstate&(~1))<<")"
                    //<<" to protect val "<<(*((T*) curr->addr))
                    //<<" (and write new-val "<<unpackValue<T>(curr)
                    <<")");

        // try to acquire locks
        // (and fail if their versions have changed since encounterTime)
        if (!curr->LockFor->tryAcquire(*encounterTime)) {
            DEBUG2 aout("thread "<<Self->UniqID<<" failed to acquire lock "<<*curr->LockFor);
            // if we fail to acquire a lock, we must
            // unlock all previous locks that we acquired
            for (AVPair* toUnlock = lockAVPairs->head; toUnlock != curr; toUnlock = toUnlock->Next) {
                toUnlock->LockFor->release();
                DEBUG2 aout("thread "<<Self->UniqID<<" releasing lock "<<*curr->LockFor<<" in lockAll (will be ver "<<(curr->LockFor->getSnapshot().version()+2)<<")");
            }
            return false;
        }
        DEBUG2 aout("thread "<<Self->UniqID<<" acquired lock "<<*curr->LockFor);
    }
    return true;
}

__INLINE__ bool tryLockWriteSet(Thread* Self) {
    return lockAll(Self, &Self->wrSet->locks.list);
//    return lockAll<long>(Self, &Self->wrSet->l.locks.list)
//        && lockAll<float>(Self, &Self->wrSet->f.locks.list);
}

// NOT TO BE INVOKED DIRECTLY
// releases all locks on addresses in a Log
/*__INLINE__*/ void releaseAll(Thread* Self, List* lockAVPairs) {
    DEBUG3 aout("releaseAll "<<*lockAVPairs);
    AVPair* const stop = lockAVPairs->put;
    for (AVPair* curr = lockAVPairs->head; curr != stop; curr = curr->Next) {
        DEBUG2 aout("thread "<<Self->UniqID<<" releasing lock "<<*curr->LockFor<<" in releaseAll (will be ver "<<(curr->LockFor->getSnapshot().version()+2)<<")");
        curr->LockFor->release();
    }
}

// can be invoked only by a transaction on the software path
// releases all locks on addresses in the write-set
__INLINE__ void releaseWriteSet(Thread* Self) {
    assert(Self->isFallback);
    releaseAll(Self, &Self->wrSet->locks.list);
//    releaseAll(Self, &Self->wrSet->l.locks.list);
//    releaseAll(Self, &Self->wrSet->f.locks.list);
}

// can be invoked only by a transaction on the software path.
// writeSet must point to the write-set for this Thread that
// contains addresses/values of type T.
//template <typename T>
__INLINE__ bool validateLockVersions(Thread* Self, List* lockAVPairs, bool holdingLocks) {
    DEBUG3 aout("validateLockVersions "<<*lockAVPairs<<" "<<debug(holdingLocks));
    assert(Self->isFallback);

    AVPair* const stop = lockAVPairs->put;
    for (AVPair* curr = lockAVPairs->head; curr != stop; curr = curr->Next) {
        // determine when we first encountered curr->addr in txload or txstore.
        // curr->rdv contains when we first encountered it in a txLOAD,
        // so we also need to compare it with any rdv stored in the WRITE-set.
        AVPair* writeLogEntry = Self->wrSet->findLock(curr->LockFor);
        vLockSnapshot *encounterTime = getMinLockSnap(curr, writeLogEntry);
        assert(encounterTime);
        
        vLock* lock = curr->LockFor;
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked() || locksnap.version() != encounterTime->version()) {
            // the address is locked, it its version number has changed
            // we abort if we are not the one who holds a lock on it, or changed its version.
            if (!holdingLocks) {
                return false; // abort if we are not holding any locks
            } else if (!Self->wrSet->containsLock(lock)) {
                return false; // abort if we are holding locks, but not this lock
            }
//            return false;
        }
    }
    return true;
}

__INLINE__ bool validateReadSet(Thread* Self, bool holdingLocks) {
    return validateLockVersions(Self, &Self->rdSet->locks.list, holdingLocks);
//    return validate<long>(Self, &Self->rdSet->l.locks.list, holdingLocks)
//        && validate<float>(Self, &Self->rdSet->f.locks.list, holdingLocks);
}

__INLINE__ intptr_t AtomicAdd(volatile intptr_t* addr, intptr_t dx) {
    intptr_t v;
    for (v = *addr; CAS(addr, v, v + dx) != v; v = *addr) {}
    return (v + dx);
}










/**
 * 
 * THREAD CLASS IMPLEMENTATION
 * 
 */

volatile long StartTally = 0;
volatile long AbortTallyHW = 0;
volatile long AbortTallySW = 0;
volatile long CommitTallyHW = 0;
volatile long CommitTallySW = 0;

Thread::Thread(long id) {
    DEBUG1 aout("new thread with id "<<id);
    memset(this, 0, sizeof(Thread)); /* Default value for most members */
    UniqID = id;
    rng = id + 1;
    xorrng[0] = rng;

    wrSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    rdSet = (TypeLogs*) malloc(sizeof(TypeLogs));
    LocalUndo = (TypeLogs*) malloc(sizeof(TypeLogs));
    wrSet->init(this, INIT_WRSET_NUM_ENTRY);
    rdSet->init(this, INIT_RDSET_NUM_ENTRY);
    LocalUndo->init(this, INIT_LOCAL_NUM_ENTRY);

    allocPtr = tmalloc_alloc(1);
    freePtr = tmalloc_alloc(1);
    assert(allocPtr);
    assert(freePtr);
}

void Thread::destroy() {
//    AtomicAdd((volatile intptr_t*)((void*) (&StartTally)), Starts);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTallySW)), AbortsSW);
    AtomicAdd((volatile intptr_t*)((void*) (&AbortTallyHW)), AbortsHW);
    AtomicAdd((volatile intptr_t*)((void*) (&CommitTallySW)), CommitsSW);
    AtomicAdd((volatile intptr_t*)((void*) (&CommitTallyHW)), CommitsHW);
    tmalloc_free(allocPtr);
    tmalloc_free(freePtr);
    wrSet->destroy();
    rdSet->destroy();
    LocalUndo->destroy();
    free(wrSet);
    free(rdSet);
    free(LocalUndo);
}










/**
 * 
 * IMPLEMENTATION OF TM OPERATIONS
 * 
 */

void TxClearRWSets(void* _Self) {
    Thread* Self = (Thread*) _Self;
    Self->wrSet->clear();
    Self->rdSet->clear();
    Self->LocalUndo->clear();
}

void TxStart(void* _Self, sigjmp_buf* envPtr, int aborted_in_software, int* ROFlag) {
    Thread* Self = (Thread*) _Self;
    Self->wrSet->clear();
    Self->rdSet->clear();
    Self->LocalUndo->clear();

    unsigned status;
    if (aborted_in_software) goto software;

//    Self->Starts++;
    Self->Retries = 0;
    Self->isFallback = 0;
//    Self->ROFlag = ROFlag;
    Self->IsRO = true;
    Self->envPtr = envPtr;
    if (HTM_ATTEMPT_THRESH <= 0) goto software;
htmretry:
    status = XBEGIN();
    if (status != _XBEGIN_STARTED) { // if we aborted
        ++Self->AbortsHW;
        if (++Self->Retries < HTM_ATTEMPT_THRESH) goto htmretry;
        else goto software;
    }
    return;
software:
    DEBUG2 aout("thread "<<Self->UniqID<<" started s/w tx attempt "<<(Self->AbortsSW+Self->CommitsSW)<<"; s/w commits so far="<<Self->CommitsSW);
    DEBUG1 if ((Self->CommitsSW % 50000) == 0) aout("thread "<<Self->UniqID<<" has committed "<<Self->CommitsSW<<" s/w txns");
    Self->isFallback = 1;
}

int TxCommit(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        // return immediately if txn is read-only
        if (Self->IsRO) {
            DEBUG2 aout("thread "<<Self->UniqID<<" commits read-only txn");
            goto success;
        }
        
        // lock all addresses in the write-set
        DEBUG2 aout("thread "<<Self->UniqID<<" invokes tryLockWriteSet "<<*Self->wrSet);
        if (!tryLockWriteSet(Self)) {
            TxAbort(Self); // abort if we fail to acquire some lock
            // (note: after lockAll we hold either all or no locks)
        }
        
        // validate reads
        if (!validateReadSet(Self, true)) { // TODO: needs to be since first time i READ OR WROTE
            // if we fail validation, release all locks and abort
            DEBUG2 aout("thread "<<Self->UniqID<<" TxCommit failed validation -> release locks & abort");
            releaseWriteSet(Self);
            TxAbort(Self);
        }
        
        // perform the actual writes
        Self->wrSet->writeForward();
        
        // release all locks
        DEBUG2 aout("thread "<<Self->UniqID<<" committed -> release locks");
        releaseWriteSet(Self);
        ++Self->CommitsSW;
        
    // hardware path
    } else {
        XEND();
        ++Self->CommitsHW;
    }
    
success:
#ifdef TXNL_MEM_RECLAMATION
    // "commit" speculative frees and speculative allocations
    tmalloc_releaseAllForward(Self->freePtr, NULL);
    tmalloc_clear(Self->allocPtr);
#endif
    return true;
}

void TxAbort(void* _Self) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
        ++Self->Retries;
        ++Self->AbortsSW;
        if (Self->AbortsSW > MAX_SW_ABORTS) { fprintf(stderr, "TOO MANY ABORTS. QUITTING.\n"); exit(-1); }
        
#ifdef TXNL_MEM_RECLAMATION
        // "abort" speculative allocations and speculative frees
        tmalloc_releaseAllReverse(Self->allocPtr, NULL);
        tmalloc_clear(Self->freePtr);
#endif
        
        // longjmp to start of txn
        SIGLONGJMP(*Self->envPtr, 1);
        ASSERT(0);
        
    // hardware path
    } else {
        XABORT(0);
    }
}

template <typename T>
T TxLoad(void* _Self, volatile T* Addr) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
//        printf("txLoad(id=%ld, addr=0x%lX) on fallback\n", Self->UniqID, (unsigned long)(void*) Addr);
        
        // check whether Addr is in the write-set
        AVPair* av = Self->wrSet->findAddr<T>((uintptr_t) Addr);
        if (av != NULL) return unpackValue<T>(av);

        // Addr is NOT in the write-set; abort if it is locked
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) {
            DEBUG2 aout("thread "<<Self->UniqID<<" TxRead saw lock "<<lock<<" was held (not by us) -> aborting");
            TxAbort(Self);
        }
        
        // read addr and add it to the read-set
        T val = *Addr;
        Self->rdSet->insert(Addr, val, lock, locksnap);

        // validate reads
        if (!validateReadSet(Self, false)) {
            DEBUG2 aout("thread "<<Self->UniqID<<" TxRead failed validation -> aborting");
            TxAbort(Self);
        }

//        printf("    txLoad(id=%ld, ...) success\n", Self->UniqID);
        return val;
        
    // hardware path
    } else {
        // abort if addr is locked
        vLock* lock = PSLOCK(Addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) TxAbort(Self);
        
        // actually read addr
        return *Addr;
    }
}

template <typename T>
__INLINE__ T TxStore(void* _Self, volatile T* addr, T valu) {
    Thread* Self = (Thread*) _Self;
    
    // software path
    if (Self->isFallback) {
//        printf("txStore(id=%ld, addr=0x%lX, val=%ld) on fallback\n", Self->UniqID, (unsigned long)(void*) addr, (long) valu);
        
        // abort if addr is locked (since we do not hold any locks)
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) {
            DEBUG2 aout("thread "<<Self->UniqID<<" TxStore saw lock "<<lock<<" was held (not by us) -> aborting");
            TxAbort(Self);
        }
        
        // add addr to the write-set
        Self->wrSet->insert<T>(addr, valu, lock, locksnap);
        Self->IsRO = false; // txn is not read-only

//        printf("    txStore(id=%ld, ...) success\n", Self->UniqID);
        return valu;
        
    // hardware path
    } else {
        // abort if addr is locked
        vLock* lock = PSLOCK(addr);
        vLockSnapshot locksnap = lock->getSnapshot();
        if (locksnap.isLocked()) XABORT(_XABORT_EXPLICIT_LOCKED);
        
        // increment version number (to notify s/w txns of the change)
        // and write value to addr (order unimportant because of h/w)
        lock->htmIncrementVersion();
        return *addr = valu;
    }
}










/**
 * 
 * FRAMEWORK FUNCTIONS
 * (PROBABLY DON'T NEED TO BE CHANGED WHEN CREATING A VARIATION OF THIS TM)
 * 
 */

void TxOnce() {
    CTASSERT((_TABSZ & (_TABSZ - 1)) == 0); /* must be power of 2 */
    printf("%s %s\n", TM_NAME, "system ready\n");
    memset(LockTab, 0, _TABSZ*sizeof(vLock));
}

void TxShutdown() {
    printf("%s system shutdown:\n  Starts=%li CommitsHW=%li AbortsHW=%li CommitsSW=%li AbortsSW=%li\n",
                TM_NAME,
                CommitTallyHW+CommitTallySW,
                CommitTallyHW, AbortTallyHW,
                CommitTallySW, AbortTallySW);
}

void* TxNewThread() {
    Thread* t = (Thread*) malloc(sizeof(Thread));
    assert(t);
    return t;
}

void TxFreeThread(void* _t) {
    Thread* t = (Thread*) _t;
    t->destroy();
    free(t);
}

void TxInitThread(void* _t, long id) {
    Thread* t = (Thread*) _t;
    *t = Thread(id);
}

/* =============================================================================
 * TxAlloc
 *
 * CCM: simple transactional memory allocation
 * =============================================================================
 */
void* TxAlloc(void* _Self, size_t size) {
#ifdef TXNL_MEM_RECLAMATION
    Thread* Self = (Thread*) _Self;
    void* ptr = tmalloc_reserve(size);
    if (ptr) {
        tmalloc_append(Self->allocPtr, ptr);
    }

    return ptr;
#else
    return malloc(size);
#endif
}

/* =============================================================================
 * TxFree
 *
 * CCM: simple transactional memory de-allocation
 * =============================================================================
 */
void TxFree(void* _Self, void* ptr) {
#ifdef TXNL_MEM_RECLAMATION
    Thread* Self = (Thread*) _Self;
    tmalloc_append(Self->freePtr, ptr);
#else
    free(ptr);
#endif
}

long TxLoadl(void* _Self, volatile long* addr) {
    Thread* Self = (Thread*) _Self;
    return TxLoad(Self, addr);
}
float TxLoadf(void* _Self, volatile float* addr) {
    Thread* Self = (Thread*) _Self;
    return TxLoad(Self, addr);
}

long TxStorel(void* _Self, volatile long* addr, long value) {
    Thread* Self = (Thread*) _Self;
    return TxStore(Self, addr, value);
}
float TxStoref(void* _Self, volatile float* addr, float value) {
    Thread* Self = (Thread*) _Self;
    return TxStore(Self, addr, value);
}
