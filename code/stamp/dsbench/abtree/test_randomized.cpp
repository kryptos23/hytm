/* 
 * File:   test_concurrent.cpp
 * Author: trbot
 *
 * Created on November 2, 2015, 7:07 PM
 */

#include <cstdlib>
#include <iostream>
#include <atomic>
#include "../debugcounters.h"
#include "../globals.h"
#include "../globals_extern.h"
#include "../recordmgr/record_manager.h"
#include "abtree_impl.h"
#include "../common/random.h"

using namespace std;

#define PASTE2(a, b) a##b
#define PASTE(a, b) PASTE2(a, b)
#define STRINGIFY2(a) #a
#define STRINGIFY(a) STRINGIFY2(a)

#define DEFAULT_SUSPECTED_SIGNAL SIGQUIT
#define ABTREE_NODE_MIN_DEGREE 2
#define ABTREE_NODE_DEGREE 4
#define DS_DECLARATION abtree<ABTREE_NODE_DEGREE, test_type, less<test_type>, MemMgmt>

#define INSERT(x, y) { \
    const void * retval = tree->insert(tid, (x), (void*) (y)); \
    if ((void *) (retval) == tree->NO_VALUE) { \
        keysum += (x); \
        cout<<"success"<<endl; \
    } else { \
        cout<<"fail"<<endl; \
    } \
    if (validateEveryTime && !tree->validate(keysum, true)) { \
        return -1; \
    } \
}

#define ERASE(x) { \
    bool succ = tree->erase(tid, (x)).second; \
    if (succ) { \
        cout<<"success"<<endl; \
        keysum -= (x); \
    } else { \
        cout<<"fail"<<endl; \
    } \
    if (validateEveryTime && !tree->validate(keysum, true)) { \
        return -1; \
    } \
}

#define ASSIGNI(var, defaultVal) \
    int var = defaultVal; \
    for (int i=0;i<argc;++i) { \
        if (strcmp(argv[i], STRINGIFY(var)) == 0) { \
            var = atof(argv[++i]); \
        } \
    }

#define PRINT(var) cout<<STRINGIFY(var)<<"="<<var<<endl;

int main(int argc, char** argv) {
    typedef long long test_type;
    typedef reclaimer_none<test_type> Reclaim;
    typedef allocator_once<test_type> Alloc;
    typedef pool_none<test_type> Pool;
    typedef record_manager<Reclaim, Alloc, Pool, abtree_Node<ABTREE_NODE_DEGREE, test_type>, abtree_SCXRecord<ABTREE_NODE_DEGREE, test_type> > MemMgmt;
    const test_type NO_KEY = -1;
    WORK_THREADS = 1;
    MAX_FAST_HTM_RETRIES = -1;
    MAX_SLOW_HTM_RETRIES = 200;
    DS_DECLARATION * tree = new DS_DECLARATION(WORK_THREADS, DEFAULT_SUSPECTED_SIGNAL, NO_KEY, ABTREE_NODE_MIN_DEGREE);

    ASSIGNI(validateEveryTime, 1);
    ASSIGNI(keyRange, 128);
    ASSIGNI(nops, 1000000);
    ASSIGNI(seed, 1718395912);
    ASSIGNI(printInterval, 100);
    
    PRINT(validateEveryTime);
    PRINT(keyRange);
    PRINT(nops);
    
    const int tid = 0;
    long long keysum = 0;
    
    TRACE_TOGGLE;
    PRINT(___trace);
    
    tree->initThread(tid);
    
    Random rng (seed);
    tree->debugPrint();
    for (int i=0;i<=nops;++i) {
        if ((i%printInterval)==0) {
            if (!validateEveryTime && !tree->validate(keysum, true)) {
                return -1;
            }
            cout<<"i="<<i<<endl;
        }

        cout<<"i="<<i<<" ";
        test_type x = rng.nextNatural(keyRange);
        if (rng.nextNatural(103) < 51) {
            cout<<"insert "<<x<<"... ";
            INSERT(x, x);
//            tree->debugPrint();
        } else {
            cout<<"erase "<<x<<"... ";
            ERASE(x);
//            tree->debugPrint();
        }

//        cout<<"  ";
//        tree->debugPrint();
//        cout<<endl;
    }
    
    return 0;
}

