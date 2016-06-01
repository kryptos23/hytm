/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX.
 * 
 * Copyright (C) 2014 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

typedef long long test_type;

#include <cstdlib>
#include <ctime>
#include <chrono>
#include <typeinfo>
#include <atomic>
#include "../common/random.h"
#include "../globals.h"
#include "../globals_extern.h"
#include "../recordmgr/machineconstants.h"

#define DEFAULT_SUSPECTED_SIGNAL SIGQUIT /* UNUSED */

#include "abtree_impl.h"
#include "../recordmgr/record_manager.h"

using namespace std;

// variables used in the concurrent test
chrono::time_point<chrono::high_resolution_clock> startTime;
chrono::time_point<chrono::high_resolution_clock> endTime;
long long elapsedMillis;
long long keysum; // key sum hashes for the main thread
long long updatecount;

const test_type NO_KEY = -1;
const test_type NO_VALUE = -1;
const int RETRY = -2;
void *__tree;

#define ABTREE_NODE_DEGREE 16
#define ABTREE_NODE_MIN_DEGREE 6
#define DS_DECLARATION abtree<ABTREE_NODE_DEGREE, test_type, less<test_type>, MemMgmt>

#define PRINTI(name) { cout<<(#name)<<"="<<name<<endl; }
#define PRINTS(name) { cout<<(#name)<<"="<<name<<endl; }

template <class Reclaim, class Alloc, class Pool>
void performExperiment() {
    // get data structure and random number generator seeded with time
    typedef record_manager<Reclaim, Alloc, Pool, abtree_Node<ABTREE_NODE_DEGREE, test_type>, abtree_SCXRecord<ABTREE_NODE_DEGREE, test_type> > MemMgmt;
    DS_DECLARATION * tree = new DS_DECLARATION(WORK_THREADS, DEFAULT_SUSPECTED_SIGNAL /* UNUSED */, NO_KEY, ABTREE_NODE_MIN_DEGREE);
    Random rng (time(NULL));

    // start trial
    const int OPS_BETWEEN_TIME_CHECKS = 500;
    int tid = 0;
    tree->initThread(tid);
    startTime = chrono::high_resolution_clock::now();
    int cnt = 0;
    
    // run trial for MILLIS_TO_RUN milliseconds
    while (1) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count() >= MILLIS_TO_RUN) {
                return;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng.nextNatural(MAXKEY);
        double op = rng.nextNatural(100000000) / 1000000.;
        if (op < INS) {
            if (tree->insert_tle(tid, key, (void *) (uint64_t) key) == tree->NO_VALUE) {
                keysum += key;
                ++updatecount;
            }
        } else {
            if (tree->erase_tle(tid, key).second) {
                keysum -= key;
                ++updatecount;
            }
        }
    }
}

template <class Reclaim, class Alloc>
void performExperiment() {
    // determine the correct pool class
    
    if (strcmp(POOL_TYPE, "perthread_and_shared") == 0) {
        performExperiment<Reclaim, Alloc, pool_perthread_and_shared<test_type> >();
    } else if (strcmp(POOL_TYPE, "none") == 0) {
        performExperiment<Reclaim, Alloc, pool_none<test_type> >();
    } else {
        cout<<"bad pool type"<<endl;
        exit(1);
    }
}

template <class Reclaim>
void performExperiment() {
    // determine the correct allocator class
    
    if (strcmp(ALLOC_TYPE, "once") == 0) {
        performExperiment<Reclaim, allocator_once<test_type> >();
    } else if (strcmp(ALLOC_TYPE, "new") == 0) {
        performExperiment<Reclaim, allocator_new<test_type> >();
    } else {
        cout<<"bad allocator type"<<endl;
        exit(1);
    }
}

void performExperiment() {
    // determine the correct reclaim class
    
    if (strcmp(RECLAIM_TYPE, "none") == 0) {
        performExperiment<reclaimer_none<test_type> >();
    } else if (strcmp(RECLAIM_TYPE, "debra") == 0) {
        performExperiment<reclaimer_debra<test_type> >();
    } else {
        cout<<"bad reclaimer type"<<endl;
        exit(1);
    }
}

int main(int argc, char** argv) {
    updatecount = 0;
    MILLIS_TO_RUN = -1;
    MAX_FAST_HTM_RETRIES = 10;
    INS = 50;
    DEL = 50;
    MAXKEY = 65536;
    WORK_THREADS = 1;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i], "-mr") == 0) {
            RECLAIM_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-ma") == 0) {
            ALLOC_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-mp") == 0) {
            POOL_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            MILLIS_TO_RUN = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-txretries") == 0) {
            MAX_FAST_HTM_RETRIES = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-trace") == 0) {
            TRACE_ON;
            cout<<"TRACING ENABLED: TRACE state is "<<___trace<<endl;
        } else {
            cout<<"bad arguments"<<endl;
            exit(1);
        }
    }
    PRINTI(MAX_FAST_HTM_RETRIES);
    PRINTI(MILLIS_TO_RUN);
    PRINTI(INS);
    PRINTI(DEL);
    PRINTI(MAXKEY);
    PRINTI(WORK_THREADS);
    PRINTS(RECLAIM_TYPE);
    PRINTS(ALLOC_TYPE);
    PRINTS(POOL_TYPE);
    
    performExperiment();
    cout<<"updatecount="<<updatecount<<endl;

    return 0;
}
