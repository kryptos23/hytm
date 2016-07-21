/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX.
 * 
 * Copyright (C) 2014 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

//#if defined(__LP64) || defined(__LP64__)
typedef long test_type; // really want compile-time assert that this is the same size as a pointer!

#include <cmath>
#include <bitset>
#include <fstream>
#include <sstream>
#include <sched.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <ctime>
#include <set>
#include <chrono>
#include <typeinfo>
#include <pthread.h>
#include <atomic>
#include <tm.h>
#include "common/random.h"
#include "globals.h"
#include "globals_extern.h"
#include "recordmgr/machineconstants.h"
#include <thread.h>

#ifndef EXPERIMENT_FN
#define EXPERIMENT_FN trial
#endif

#define DEFAULT_SUSPECTED_SIGNAL SIGQUIT
static Random rngs[MAX_TID_POW2*PREFETCH_SIZE_WORDS]; // create per-thread random number generators (padded to avoid false sharing)

#if defined(BST)
#include "bst/bst_impl.h"
#include "recordmgr/record_manager.h"
//#elif defined(MCLIST)
//#include "mcas/List/MC_list_impl.h"
//#define INSERT_IF_ABSENT
#elif defined(ABTREE)
#include "abtree/abtree_impl.h"
#include "recordmgr/record_manager.h"
//#elif defined(CITRUS)
//#include "recordmgr/record_manager.h"
//#include "citrus/prcu.h"
//#include "citrus/citrus_impl.h"
#else
#error "Failed to define a data structure"
#endif

using namespace std;

// variables used in the concurrent test
chrono::time_point<chrono::high_resolution_clock> startTime;
chrono::time_point<chrono::high_resolution_clock> endTime;
long elapsedMillis;
bool start = false;
bool done = false;
atomic_int running; // number of threads that are running
debugCounter * keysum; // key sum hashes for all threads (including for prefilling)

// create a binary search tree with an allocator that uses a new form of epoch based memory reclamation
const test_type NO_KEY = -1;
const test_type NO_VALUE = -1;
const int RETRY = -2;
void *__tree;

const test_type NEG_INFTY = -1;
const test_type POS_INFTY = 2000000000;

#if defined(BST)
#define DS_DECLARATION bst<test_type, test_type, less<test_type>, MemMgmt>
//#elif defined(MCLIST)
//#define DS_DECLARATION MC_list<test_type, test_type, MemMgmt>
#elif defined(ABTREE)
#define ABTREE_NODE_DEGREE 16
#define ABTREE_NODE_MIN_DEGREE 6
#define DS_DECLARATION abtree<ABTREE_NODE_DEGREE, test_type, less<test_type>, MemMgmt>
//#elif defined(CITRUS)
//#define DS_DECLARATION citrustree<MemMgmt>
#else
#error "Failed to define a data structure"
#endif

#define STR(x) XSTR(x)
#define XSTR(x) #x

//#ifndef RQ_FUNC
//#define RQ_FUNC rangeQuery
//#endif
//
//#ifndef INSERT_FUNC
//#define INSERT_FUNC insert
//#endif
//
//#ifndef ERASE_FUNC
//#define ERASE_FUNC erase
//#endif
//
//#ifndef RQ_FUNC
//#define RQ_FUNC rangeQuery
//#endif
//
//#ifndef FIND_FUNC
//#define FIND_FUNC find
//#endif

#if defined(ABTREE)
#define VALUE ((void*) (int64_t) key)
#define KEY keys[0]
#else
#define VALUE key
#define KEY key
#endif

#if defined(BST) || defined(ABTREE)
#if defined(TLE) || defined(tle)
        #define INSERT_AND_CHECK_SUCCESS tree->insert_tle(tid, key, VALUE) == tree->NO_VALUE
        #define DELETE_AND_CHECK_SUCCESS tree->erase_tle(tid, key).second
        #define FIND_AND_CHECK_SUCCESS tree->find_tle(tid, key)
    #else
        #define INSERT_AND_CHECK_SUCCESS tree->insert_stm(TM_ARG_ALONE, tid, key, VALUE) == tree->NO_VALUE
        #define DELETE_AND_CHECK_SUCCESS tree->erase_stm(TM_ARG_ALONE, tid, key).second
        #define FIND_AND_CHECK_SUCCESS tree->find_stm(TM_ARG_ALONE, tid, key)
    #endif

    //#define RQ_AND_CHECK_SUCCESS(rqcnt) rqcnt = tree->RQ_FUNC(tid, key, key+RQSIZE, rqResults)
    //#define RQ_GARBAGE(rqcnt) rqResults[0]->KEY + rqResults[rqcnt-1]->KEY
    #define INIT_THREAD(tid) tree->initThread(tid)
    #define PRCU_INIT 
    #define PRCU_REGISTER(tid)
    #define PRCU_UNREGISTER
    #define CLEAR_COUNTERS tree->clearCounters()
#endif

// cpu sets for binding threads to cores
#ifdef THREAD_BINDING
cpu_set_t *cpusets[PHYSICAL_PROCESSORS];
#endif

//template <class MemMgmt>
//void prefill(DS_DECLARATION * tree) {
//    const double PREFILL_THRESHOLD = 0.03;
//    for (int tid=0;tid<TOTAL_THREADS;++tid) {
//        INIT_THREAD(tid); // it must be okay that we do this with the main thread and later with another thread.
//    }
//    Random& rng = rngs[0];
//    const double expectedFullness = (INS+DEL ? INS / (double)(INS+DEL) : 0.5); // percent full in expectation
//    const int expectedSize = (int)(MAXKEY * expectedFullness);
//
//#if defined(BST) || defined(ABTREE) //|| defined(CITRUS)
//    int sz = 0;
//    int tid = 0;
//    for (int i=0;;++i) {
//        VERBOSE {
//            if (i&&((i % 1000000) == 0)) COUTATOMIC("PREFILL op# "<<i<<" sz="<<sz<<" expectedSize="<<expectedSize<<endl);
//        }
//        
//        int key = rng.nextNatural(MAXKEY);
//        int op = rng.nextNatural(100);
//        if (op < 100*expectedFullness) {
//            if (INSERT_AND_CHECK_SUCCESS) {
//                keysum->add(tid, key);
//                ++sz;
//            }
//        } else {
//            if (DELETE_AND_CHECK_SUCCESS) {
//                keysum->add(tid, -key);
//                --sz;
//            }
//        }
//        int absdiff = (sz < expectedSize ? expectedSize - sz : sz - expectedSize);
//        if (absdiff < expectedSize*PREFILL_THRESHOLD) {
//            break;
//        }
//    }
////#elif defined(MCLIST)
////    bool present[MAXKEY];
////    for (int i=0;i<MAXKEY;++i) present[i] = 0;
////    
////    int sz = 0;
////    int tid = 0;
////    for (int i=0;;++i) {
////        VERBOSE {
////            if (i&&((i % 1000000) == 0)) COUTATOMIC("PREFILL op# "<<i<<" sz="<<sz<<" expectedSize="<<expectedSize<<endl);
////        }
////        int key = rng.nextNatural(MAXKEY);
////        if (!present[key]) {
////            present[key] = 1;
////            if (++sz >= expectedSize) break;
////        }
////    }
////    // always insert at the head of the list for efficiency
////    for (int i=MAXKEY-1;i>=0;--i) {
////        if (present[i]) {
////            int key = i;
////            if (INSERT_AND_CHECK_SUCCESS) {
////                keysum->add(tid, key);
////            } else {
////                cerr<<"fatal error during prefilling: key should have been inserted"<<endl;
////                exit(-1);
////            }
////        }
////    }
//#endif
//    
//    CLEAR_COUNTERS;
//    VERBOSE COUTATOMIC("finished prefilling to size "<<sz<<" for expected size "<<expectedSize<<endl);
//}


template <class MemMgmt>
void thread_timed(void *unused) {
    const int OPS_BETWEEN_TIME_CHECKS = 500;
    int tid = thread_getId();
#ifdef THREAD_BINDING
    sched_setaffinity(0, CPU_ALLOC_SIZE(PHYSICAL_PROCESSORS), cpusets[tid%PHYSICAL_PROCESSORS]); // bind thread to core
    for (int i=0;i<PHYSICAL_PROCESSORS;++i) {
        if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(PHYSICAL_PROCESSORS), cpusets[tid%PHYSICAL_PROCESSORS])) {
            VERBOSE COUTATOMICTID("binding to cpu "<<i<<endl);
        }
    }
#endif
    
    TM_THREAD_ENTER();
    PRCU_REGISTER(tid);
    test_type garbage = 0;
    Random *rng = &rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * tree = (DS_DECLARATION *) __tree;

#if defined(BST)
    Node<test_type, test_type> const ** rqResults = new Node<test_type, test_type> const *[RQSIZE];
//#elif defined(MCLIST)
//    Node<test_type, test_type> const ** rqResults = new Node<test_type, test_type> const *[RQSIZE];
#elif defined(ABTREE)
    abtree_Node<ABTREE_NODE_DEGREE, test_type> const ** rqResults = new abtree_Node<ABTREE_NODE_DEGREE, test_type> const *[RQSIZE];
//#elif defined(CITRUS)
//    int * rqResults = new int[RQSIZE];
#endif
    
    INIT_THREAD(tid);
    running.fetch_add(1);
    __sync_synchronize();
//    while (!start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    int cnt = 0;
    while (!done) {
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            if (chrono::duration_cast<chrono::milliseconds>(__endTime-startTime).count() >= MILLIS_TO_RUN) {
                done = true;
                __sync_synchronize();
                break;
            }
        }
        
        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY);
        //cout<<"MAXKEY="<<MAXKEY<<" random key for tid="<<tid<<" is "<<key<<endl;
        double op = rng->nextNatural(100000000) / 1000000.;
        if (op < INS) {
            if (INSERT_AND_CHECK_SUCCESS) {
                keysum->add(tid, key);
            }
            tree->debugGetCounters()->insertSuccess->inc(tid);
        } else if (op < INS+DEL) {
            if (DELETE_AND_CHECK_SUCCESS) {
                keysum->add(tid, -key);
            }
            tree->debugGetCounters()->eraseSuccess->inc(tid);
        } else if (op < INS+DEL+RQ) {
//            int rqcnt;
//            if (RQ_AND_CHECK_SUCCESS(rqcnt)) { // prevent rqResults and count from being optimized out
//                garbage += RQ_GARBAGE(rqcnt);
//            }
//            tree->debugGetCounters()->rqSuccess->inc(tid);
        } else {
            FIND_AND_CHECK_SUCCESS;
            tree->debugGetCounters()->findSuccess->inc(tid);
        }
    }
    running.fetch_add(-1);
    VERBOSE COUTATOMICTID("termination"<<" garbage="<<garbage<<endl);
    
    PRCU_UNREGISTER;
    TM_THREAD_EXIT();
}

//template <class MemMgmt>
//void *thread_rq(void *_id) {
//    const int OPS_BETWEEN_TIME_CHECKS = 50;
//    int tid = *((int*) _id);
//#ifdef THREAD_BINDING
//    sched_setaffinity(0, CPU_ALLOC_SIZE(PHYSICAL_PROCESSORS), cpusets[tid%PHYSICAL_PROCESSORS]); // bind thread to core
//    for (int i=0;i<PHYSICAL_PROCESSORS;++i) {
//        if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(PHYSICAL_PROCESSORS), cpusets[tid%PHYSICAL_PROCESSORS])) {
//            VERBOSE COUTATOMICTID("binding to cpu "<<i<<endl);
//        }
//    }
//#endif
//    PRCU_REGISTER(tid);
//    test_type garbage = 0;
//    Random *rng = &rngs[tid*PREFETCH_SIZE_WORDS];
//    DS_DECLARATION * tree = (DS_DECLARATION *) __tree;
//
//#if defined(BST)
//    Node<test_type, test_type> const ** rqResults = new Node<test_type, test_type> const *[RQSIZE];
//#elif defined(MCLIST)
//    Node<test_type, test_type> const ** rqResults = new Node<test_type, test_type> const *[RQSIZE];
//#elif defined(ABTREE)
//    abtree_Node<ABTREE_NODE_DEGREE, test_type> const ** rqResults = new abtree_Node<ABTREE_NODE_DEGREE, test_type> const *[RQSIZE];
//#elif defined(CITRUS)
//    int * rqResults = new int[RQSIZE];
//#else
//#error "Failed to define a data structure"
//#endif
//    
//    int sqrt_rqsize = sqrt(RQSIZE);
//
//    INIT_THREAD(tid);
//    running.fetch_add(1);
//    __sync_synchronize();
//    while (!start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
//    int cnt = 0;
//    while (!done) {
//        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
//            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
//            if (chrono::duration_cast<chrono::milliseconds>(__endTime-startTime).count() >= MILLIS_TO_RUN) {
//                done = true;
//                __sync_synchronize();
//                break;
//            }
//        }
//        
//        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
//        int key = rng->nextNatural(MAXKEY);
//        int sz = rng->nextNatural(sqrt_rqsize); sz *= sz;
//
//        int rqcnt;
//        if (RQ_AND_CHECK_SUCCESS(rqcnt)) { // prevent rqResults and count from being optimized out
//            garbage += RQ_GARBAGE(rqcnt);
//        }
//        tree->debugGetCounters()->rqSuccess->inc(tid);
//    }
//    running.fetch_add(-1);
//    VERBOSE COUTATOMICTID("termination"<<" garbage="<<garbage<<endl);
//    PRCU_UNREGISTER;
//    return NULL;
//}
//
//template <class MemMgmt>
//void trial_nmixed() {
//#if defined(BST)
//    __tree = (void*) new DS_DECLARATION(NO_KEY, NO_VALUE, RETRY, TOTAL_THREADS);
//#elif defined(MCLIST)
//    __tree = (void*) new DS_DECLARATION(TOTAL_THREADS, NEG_INFTY, POS_INFTY, NO_VALUE);
//#elif defined(ABTREE)
//    __tree = (void*) new DS_DECLARATION(TOTAL_THREADS, DEFAULT_SUSPECTED_SIGNAL, NO_KEY, ABTREE_NODE_MIN_DEGREE);
//#elif defined(CITRUS)
//    __tree = (void*) new DS_DECLARATION(MAXKEY, TOTAL_THREADS);
//#else
//#error "Failed to define a data structure"
//#endif
//
//    // get random number generator seeded with time
//    // we use this rng to seed per-thread rng's that use a different algorithm
//    srand(time(NULL));
//
//    pthread_t *threads[WORK_THREADS];
//    int ids[WORK_THREADS];
//    for (int i=0;i<WORK_THREADS;++i) {
//        threads[i] = new pthread_t;
//        ids[i] = i;
//        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
//    }
//    
//    PRCU_INIT;    
//    if (PREFILL) prefill((DS_DECLARATION *) __tree);
//    
//    for (int i=0;i<WORK_THREADS;++i) {
//        if (pthread_create(threads[i], NULL, thread_timed<MemMgmt>, &ids[i])) {
//            cerr<<"ERROR: could not create thread"<<endl;
//            exit(-1);
//        }
//    }
//
//    while (running.load() < WORK_THREADS) {
//        TRACE COUTATOMIC("main thread: waiting for threads to START running="<<running.load()<<endl);
//    } // wait for all threads to be ready
//    COUTATOMIC("main thread: starting timer..."<<endl);
//    __sync_synchronize();
//    startTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    start = true;
//    for (int i=0;i<WORK_THREADS;++i) {
//        VERBOSE COUTATOMIC("main thread: attempting to join thread "<<i<<endl);
//        if (pthread_join(*threads[i], NULL)) {
//            cerr<<"ERROR: could not join thread"<<endl;
//            exit(-1);
//        }
//        VERBOSE COUTATOMIC("main thread: joined thread "<<i<<endl);
//    }
//    endTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
//    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
//    
//    for (int i=0;i<WORK_THREADS;++i) {
//        delete threads[i];
//    }
//}
//
//template <class MemMgmt>
//void trial_1rq_restmixed() {
//#if defined(BST)
//    __tree = (void*) new DS_DECLARATION(NO_KEY, NO_VALUE, RETRY, WORK_THREADS, DEFAULT_SUSPECTED_SIGNAL);
//#elif defined(MCLIST)
//    __tree = (void*) new DS_DECLARATION(WORK_THREADS, NEG_INFTY, POS_INFTY, NO_VALUE);
//#elif defined(ABTREE)
//    __tree = (void*) new DS_DECLARATION(WORK_THREADS, DEFAULT_SUSPECTED_SIGNAL, NO_KEY, ABTREE_NODE_MIN_DEGREE);
//#elif defined(CITRUS)
//    __tree = (void*) new DS_DECLARATION(MAXKEY, WORK_THREADS);
//#else
//#error "Failed to define a data structure"
//#endif
//
//    // get random number generator seeded with time
//    // we use this rng to seed per-thread rng's that use a different algorithm
//    srand(time(NULL));
//
//    // create range query thread (id=WORK_THREADS-1)
//    pthread_t *threads[WORK_THREADS];
//    int ids[WORK_THREADS];
//    threads[WORK_THREADS-1] = new pthread_t;
//    ids[WORK_THREADS-1] = WORK_THREADS-1;
//    rngs[(WORK_THREADS-1)*PREFETCH_SIZE_WORDS].setSeed(rand());
//
//    // create other threads (id=0..WORK_THREADS-2)
//    for (int i=0;i<WORK_THREADS-1;++i) {
//        threads[i] = new pthread_t;
//        ids[i] = i;
//        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
//    }
//    
//    PRCU_INIT;
//    if (PREFILL) prefill((DS_DECLARATION *) __tree);
//    
//    // start range query thread (id=WORK_THREADS-1)
//    if (pthread_create(threads[WORK_THREADS-1], NULL, thread_rq<MemMgmt>, &ids[WORK_THREADS-1])) {
//        cerr<<"ERROR: could not create thread"<<endl;
//        exit(-1);
//    }
//    
//    // start other threads (id=0..WORK_THREADS-2)
//    for (int i=0;i<WORK_THREADS-1;++i) {
//        if (pthread_create(threads[i], NULL, thread_timed<MemMgmt>, &ids[i])) {
//            cerr<<"ERROR: could not create thread"<<endl;
//            exit(-1);
//        }
//    }
//
//    while (running.load() < WORK_THREADS) {
//        TRACE COUTATOMIC("main thread: waiting for threads to START running="<<running.load()<<endl);
//    } // wait for all threads to be ready
//    COUTATOMIC("main thread: starting timer..."<<endl);
//    __sync_synchronize();
//    startTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    start = true;
//    for (int i=0;i<WORK_THREADS;++i) {
//        VERBOSE COUTATOMIC("main thread: attempting to join thread "<<i<<endl);
//        if (pthread_join(*threads[i], NULL)) {
//            cerr<<"ERROR: could not join thread"<<endl;
//            exit(-1);
//        }
//        VERBOSE COUTATOMIC("main thread: joined thread "<<i<<endl);
//    }
//    endTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
//    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
//    
//    for (int i=0;i<WORK_THREADS;++i) {
//        delete threads[i];
//    }
//}

template <class MemMgmt>
void trial() {
#if defined(BST)
    __tree = (void*) new DS_DECLARATION(NO_KEY, NO_VALUE, RETRY, TOTAL_THREADS);
//#elif defined(MCLIST)
//    __tree = (void*) new DS_DECLARATION(TOTAL_THREADS, NEG_INFTY, POS_INFTY, NO_VALUE);
#elif defined(ABTREE)
    __tree = (void*) new DS_DECLARATION(TOTAL_THREADS, DEFAULT_SUSPECTED_SIGNAL, NO_KEY, ABTREE_NODE_MIN_DEGREE);
//#elif defined(CITRUS)
//    __tree = (void*) new DS_DECLARATION(MAXKEY, TOTAL_THREADS);
#else
#error "Failed to define a data structure"
#endif
    
    // get random number generator seeded with time
    // we use this rng to seed per-thread rng's that use a different algorithm
    srand(time(NULL));
    for (int i=0;i<TOTAL_THREADS;++i) {
        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
    }
    
    assert(RQ_THREADS == 0);
    TM_STARTUP(TOTAL_THREADS);
    thread_startup(TOTAL_THREADS);
    
    startTime = chrono::high_resolution_clock::now();
    __sync_synchronize();
    
    thread_start(thread_timed<MemMgmt>, NULL);
    
    endTime = chrono::high_resolution_clock::now();
    __sync_synchronize();
    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
    
    TM_SHUTDOWN();
    thread_shutdown();
    
//    // create threads
//    pthread_t *threads[TOTAL_THREADS];
//    for (int i=0;i<TOTAL_THREADS;++i) {
//        threads[i] = new pthread_t;
//        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
//    }
//
//    if (PREFILL) prefill((DS_DECLARATION *) __tree);
//
//    // start all threads
//    for (int i=0;i<TOTAL_THREADS;++i) {
//        if (pthread_create(threads[i], NULL,
//                    (i < WORK_THREADS
//                     ? thread_timed<MemMgmt>
//                     : thread_rq<MemMgmt>), &ids[i])) {
//            cerr<<"ERROR: could not create thread"<<endl;
//            exit(-1);
//        }
//    }
//
//    while (running.load() < TOTAL_THREADS) {
//        TRACE COUTATOMIC("main thread: waiting for threads to START running="<<running.load()<<endl);
//    } // wait for all threads to be ready
//    COUTATOMIC("main thread: starting timer..."<<endl);
//    __sync_synchronize();
//    startTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    start = true;
//    for (int i=0;i<TOTAL_THREADS;++i) {
//        VERBOSE COUTATOMIC("main thread: attempting to join thread "<<i<<endl);
//        if (pthread_join(*threads[i], NULL)) {
//            cerr<<"ERROR: could not join thread"<<endl;
//            exit(-1);
//        }
//        VERBOSE COUTATOMIC("main thread: joined thread "<<i<<endl);
//    }
//    endTime = chrono::high_resolution_clock::now();
//    __sync_synchronize();
//    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
//    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
//
//    for (int i=0;i<TOTAL_THREADS;++i) {
//        delete threads[i];
//    }
}

#ifdef BST
//void printInterruptedAddresses(ostream& os) {
//    // print addresses where threads were interrupted by signals
//    // that performed siglongjmp
//    for (int tid=0;tid<TOTAL_THREADS;++tid) {
//        const int sz = threadAddrIndices[tid*PREFETCH_SIZE_WORDS];
//        for (int ix=0;ix<sz;++ix) {
//            os<<hex<<threadAddr[tid*MAX_THREAD_ADDR + ix]<<endl;
//        }
//    }
//}

void sighandler(int signum) {
    printf("Process %d got signal %d\n", getpid(), signum);

#ifdef POSIX_SYSTEM    
    if (signum == SIGUSR1) {
        TRACE_TOGGLE;
    } else {
        // tell all threads to terminate
        done = true;
        __sync_synchronize();

        // wait up to 10 seconds for all threads to terminate
        auto t1 = chrono::high_resolution_clock::now();
        while (running.load() > 0) {
            auto t2 = chrono::high_resolution_clock::now();
            auto diffSecondsFloor = chrono::duration_cast<chrono::milliseconds>(t2-t1).count();
            if (diffSecondsFloor > 3000) {
                COUTATOMIC("WARNING: threads did not terminate after "<<diffSecondsFloor<<"ms; forcefully exiting; tree render may not be consistent... running="<<running.load()<<endl);
                break;
            }
        }

        // print a tree file that we can later render an image from
        bst<test_type, test_type, less<test_type>, void> *tree =
        ((bst<test_type, test_type, less<test_type>, void> *) __tree);
        tree->debugPrintToFile("tree", 0, "", 0, "");
        tree->debugPrintToFileWeight("tree", 0, "", 0, "weight");

        fstream fs ("addr", fstream::out);
//        printInterruptedAddresses(fs);
        fs.close();

        // handle the original signal to get a core dump / termination as necessary
        if (signum == SIGSEGV || signum == SIGABRT) {
            signal(signum, SIG_DFL);
            kill(getpid(), signum);
        } else {
            printf("exiting...\n");
            exit(-1);
        }
    }
#endif
}
#endif

string percent(int numer, int denom) {
    if (numer == 0 || denom == 0) return "0%";
//    if (numer == 0 || denom == 0) return "0.0%";
    double x = numer / (double) denom;
    stringstream ss;
    int xpercent = (int)(x*100+0.5);
    ss<<xpercent<<"%";
    return ss.str();
}

template <class MemMgmt>
void printOutput() {
    DS_DECLARATION * tree = (DS_DECLARATION *) __tree;

    long long threadsKeySum = keysum->getTotal();
    long long treeKeySum = tree->debugKeySum();
    if (threadsKeySum == treeKeySum) {
        cout<<"Validation OK: threadsKeySum="<<threadsKeySum<<" treeKeySum="<<treeKeySum<<endl;
    } else {
        cout<<"Validation FAILURE: threadsKeySum="<<threadsKeySum<<" treeKeySum="<<treeKeySum<<endl;
#if defined(ABTREE) || defined(BST) || defined(MCLIST)
        tree->debugPrintToFile("tree_", 0, "", 0, ".out");
#endif
        exit(-1);
    }

    debugCounters * const counters = tree->debugGetCounters();
    int totalOps = 0, totalSuccessful = 0, totalChanges = 0;
    int totalHTMAbort[NUMBER_OF_PATHS];
    int totalPathOps[NUMBER_OF_PATHS];
    for (int path=0;path<NUMBER_OF_PATHS;++path) {
        totalHTMAbort[path] = 0;
#ifdef RECORD_ABORTS
        for (int i=0;i<MAX_ABORT_STATUS;++i) {
            int _total = counters->htmAbort[path*MAX_ABORT_STATUS+i]->getTotal();
            totalHTMAbort[path] += _total;
        }
#endif
        totalSuccessful += counters->pathSuccess[path]->getTotal();
        totalChanges += counters->updateChange[path]->getTotal();
        totalPathOps[path] = counters->pathSuccess[path]->getTotal() + counters->pathFail[path]->getTotal();
        totalOps += totalPathOps[path];
    }
    
    for (int path=0;path<NUMBER_OF_PATHS;++path) {
//        switch (path) {
//            case PATH_FAST_HTM: if (MAX_FAST_HTM_RETRIES >= 0) COUTATOMIC("[" << PATH_NAMES[path] << " = " << P1NAME << "]" << endl); break;
//            case PATH_SLOW_HTM: if (MAX_SLOW_HTM_RETRIES >= 0) COUTATOMIC("[" << PATH_NAMES[path] << " = " << P2NAME << "]" << endl); break;
//            case PATH_FALLBACK: COUTATOMIC("[" << PATH_NAMES[path] << " = " << P3NAME << "]" << endl); break;
//        }
        if (totalPathOps[path] > 0) {
            COUTATOMIC("total path "<<PATH_NAMES[path]<<"           : "<<totalPathOps[path]<<" ("<<percent(totalPathOps[path], totalOps)<<" of ops)"<<endl);
        }
        if (counters->htmCommit[path]->getTotal() + totalHTMAbort[path] > 0) {
            COUTATOMIC("total "<<PATH_NAMES[path]<<" commit         : "<<counters->htmCommit[path]->getTotal()<<" ("<<percent(counters->htmCommit[path]->getTotal(), totalPathOps[path])<<" of "<<PATH_NAMES[path]<<" ops)"<<endl);
            COUTATOMIC("total "<<PATH_NAMES[path]<<" abort          : "<<totalHTMAbort[path]<<" ("<<percent(totalHTMAbort[path], totalPathOps[path])<<" of "<<PATH_NAMES[path]<<" ops)"<<endl);
#ifdef RECORD_ABORTS
            for (int i=0;i<MAX_ABORT_STATUS;++i) {
                int _total = counters->htmAbort[path*MAX_ABORT_STATUS+i]->getTotal();
                if (_total) {
                    COUTATOMIC("    "<<PATH_NAMES[path]<<" abort            : "<<(i==0?"unexplained":getAllAbortNames(i))<<" "<<_total<<" ("<<percent(_total, totalHTMAbort[path])<<" of "<<PATH_NAMES[path]<<" aborts, "<<percent(_total, totalPathOps[path])<<" of "<<PATH_NAMES[path]<<" ops)"<<endl);
                }
            }
#endif
        }
        if (totalPathOps[path] > 0) {
            if (counters->pathSuccess[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" success              : "<<counters->pathSuccess[path]->getTotal()<<" ("<<percent(counters->pathSuccess[path]->getTotal(), totalSuccessful)<<" of successful)"<<endl);
            if (counters->updateChange[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" changes              : "<<counters->updateChange[path]->getTotal()<<" ("<<percent(counters->updateChange[path]->getTotal(), totalChanges)<<" of changes)"<<endl);
            if (counters->pathFail[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" fail                 : "<<counters->pathFail[path]->getTotal()<<endl);
//            if (counters->rqSuccess[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" rq succ              : "<<counters->rqSuccess[path]->getTotal()<<endl);
//            if (counters->rqFail[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" rq fail              : "<<counters->rqFail[path]->getTotal()<<endl);
            if (counters->htmCapacityAbortThenCommit[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" capacity -> commit   : "<<counters->htmCapacityAbortThenCommit[path]->getTotal()<<endl);
            if (counters->htmRetryAbortRetried[path]->getTotal()) COUTATOMIC(PATH_NAMES[path]<<" retry -> try again   : "<<counters->htmRetryAbortRetried[path]->getTotal()<<endl);
        }
    }
    COUTATOMIC(endl);
    

//    COUTATOMIC("total llx true                : "<<counters->llxSuccess->getTotal()<<endl);
//    COUTATOMIC("total llx false               : "<<counters->llxFail->getTotal()<<endl);
    long long rqSuccessTotal = counters->rqSuccess->getTotal();
    long long rqFailTotal = counters->rqFail->getTotal();
    long long findSuccessTotal = counters->findSuccess->getTotal();
    long long findFailTotal = counters->findFail->getTotal();
//    for (int path=0;path<NUMBER_OF_PATHS;++path) {
//        rqSuccessTotal += counters->rqSuccess[path]->getTotal();
//        rqFailTotal += counters->rqFail[path]->getTotal();
//        findSuccessTotal += counters->findSuccess[path]->getTotal();
//        findFailTotal += counters->findFail[path]->getTotal();
//    }
    if (counters->insertSuccess->getTotal()) COUTATOMIC("total insert succ             : "<<counters->insertSuccess->getTotal()<<endl);
    if (counters->insertFail->getTotal()) COUTATOMIC("total insert retry            : "<<counters->insertFail->getTotal()<<endl);
    if (counters->eraseSuccess->getTotal()) COUTATOMIC("total erase succ              : "<<counters->eraseSuccess->getTotal()<<endl);
    if (counters->eraseFail->getTotal()) COUTATOMIC("total erase retry             : "<<counters->eraseFail->getTotal()<<endl);
    if (findSuccessTotal) COUTATOMIC("total find succ               : "<<findSuccessTotal<<endl);
    if (findFailTotal) COUTATOMIC("total find retry              : "<<findFailTotal<<endl);
    if (rqSuccessTotal) COUTATOMIC("total rq succ                 : "<<rqSuccessTotal<<endl);
    if (rqFailTotal) COUTATOMIC("total rq fail                 : "<<rqFailTotal<<endl);
    const long totalSuccUpdates = counters->insertSuccess->getTotal()+counters->eraseSuccess->getTotal();
    const long totalSuccAll = totalSuccUpdates + rqSuccessTotal + findSuccessTotal;
    const long throughput = (long) (totalSuccUpdates / (elapsedMillis/1000.));
    const long throughputAll = (long) (totalSuccAll / (elapsedMillis/1000.));
    COUTATOMIC("total succ updates            : "<<totalSuccUpdates<<endl);
    COUTATOMIC("total succ                    : "<<totalSuccAll<<endl);
    COUTATOMIC("throughput (succ updates/sec) : "<<throughput<<endl);
    COUTATOMIC("    incl. queries             : "<<throughputAll<<endl);
    COUTATOMIC("elapsed milliseconds          : "<<elapsedMillis<<endl);
    COUTATOMIC(endl);

    COUTATOMIC(endl);
   
#if defined(BST) || defined(ABTREE) || defined(MCLIST)
    if (PRINT_TREE) {
        tree->debugPrintToFile("tree_", 0, "", 0, ".out");
    }
    
    // free tree
    delete tree;
#endif
    VERBOSE COUTATOMIC("main thread: garbage#=");
    VERBOSE COUTATOMIC(counters->garbage->getTotal()<<endl);
}

#define PRINTI(name) { cout<<(#name)<<"="<<name<<endl; }
#define PRINTS(name) { cout<<(#name)<<"="<<name<<endl; }

template <class Reclaim, class Alloc, class Pool>
void performExperiment() {
#if defined(BST)
    typedef record_manager<Reclaim, Alloc, Pool, Node<test_type, test_type>, SCXRecord<test_type, test_type> > MemMgmt;
#elif defined(ABTREE)
    typedef record_manager<Reclaim, Alloc, Pool, abtree_Node<ABTREE_NODE_DEGREE, test_type>, abtree_SCXRecord<ABTREE_NODE_DEGREE, test_type> > MemMgmt;
//#elif defined(MCLIST)
//    typedef record_manager<Reclaim, Alloc, Pool, Node<test_type, test_type> > MemMgmt;
//#elif defined(CITRUS)
//    typedef record_manager<Reclaim, Alloc, Pool, node_t> MemMgmt;
#endif
    EXPERIMENT_FN<MemMgmt>();
    printOutput<MemMgmt>();
}

template <class Reclaim, class Alloc>
void performExperiment() {
    // determine the correct pool class
    
/*    if (strcmp(POOL_TYPE, "perthread_and_shared") == 0) {
        performExperiment<Reclaim, Alloc, pool_perthread_and_shared<test_type> >();
    } else*/ if (strcmp(POOL_TYPE, "none") == 0) {
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
//    } else if (strcmp(ALLOC_TYPE, "bump") == 0) {
//        performExperiment<Reclaim, allocator_bump<test_type> >();
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
#ifdef THREAD_BINDING
    const int NONE = 0;
    const int IDENTITY = 1;
    const int X52_SCATTER = 2; // specific to oracle x5-2
    const int X52_SOCKET1_THEN_SOCKET2 = 3; // specific to oracle x5-2

    // create cpu sets for binding threads to cores
    int size = CPU_ALLOC_SIZE(PHYSICAL_PROCESSORS);
    for (int i=0;i<PHYSICAL_PROCESSORS;++i) {
        cpusets[i] = CPU_ALLOC(PHYSICAL_PROCESSORS);
        CPU_ZERO_S(size, cpusets[i]);
        int j = -1;
        switch (THREAD_BINDING) {
            case IDENTITY:
                j = i;
                break;
            case X52_SCATTER:
                if (i >= PHYSICAL_PROCESSORS / 2) { // hyperthreading
                    j = i - PHYSICAL_PROCESSORS / 2;
                } else {
                    j = i;
                }
                if (i & 1) j++;
                j /= 2;
                if (i & 1) j--;
                if (i & 1) { // odd
                    j += PHYSICAL_PROCESSORS / 4;
                }
                if (i >= PHYSICAL_PROCESSORS / 2) { // hyperthreading
                    j += PHYSICAL_PROCESSORS / 2;
                }
                break;
            case X52_SOCKET1_THEN_SOCKET2:
                j = i;
                if (j >= PHYSICAL_PROCESSORS / 4 && j < PHYSICAL_PROCESSORS / 2) {
                    j += 18;
                } else if (j >= PHYSICAL_PROCESSORS / 2 && j < 3 * PHYSICAL_PROCESSORS / 4) {
                    j -= 18;
                }
                break;
        }
        if (THREAD_BINDING != NONE) {
            CPU_SET_S(j, size, cpusets[i]);
        }
    }
#endif

    PREFILL = false;
    MILLIS_TO_RUN = -1;
    MAX_FAST_HTM_RETRIES = 10;
    MAX_SLOW_HTM_RETRIES = -1;
    RQSIZE = 0;
    RQ = 0;
    RQ_THREADS = 0;
    PRINT_TREE = false;
//    NO_THREADS = false;
    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i], "-i") == 0) {
            INS = atof(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            DEL = atof(argv[++i]);
        } else if (strcmp(argv[i], "-rq") == 0) {
            RQ = atof(argv[++i]);
        } else if (strcmp(argv[i], "-rqsize") == 0) {
            RQSIZE = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0) {
            MAXKEY = atoi(argv[++i]);
//        } else if (strcmp(argv[i], "-nrq") == 0) {
//            RQ_THREADS = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-nwork") == 0) {
            WORK_THREADS = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0) {
            OPS_PER_THREAD = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-mr") == 0) {
            RECLAIM_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-ma") == 0) {
            ALLOC_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-mp") == 0) {
            POOL_TYPE = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            MILLIS_TO_RUN = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0) {
            PREFILL = true;
//        } else if (strcmp(argv[i], "-htmfast") == 0) {
//            MAX_FAST_HTM_RETRIES = atoi(argv[++i]);
//        } else if (strcmp(argv[i], "-htmslow") == 0) {
//            MAX_SLOW_HTM_RETRIES = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-print") == 0) {
            PRINT_TREE = true;
        } else if (strcmp(argv[i], "-trace") == 0) {
            TRACE_ON;
            cout<<"TRACING ENABLED: TRACE state is "<<___trace<<endl;
        } else if (strcmp(argv[i], "-validateops") == 0) {
            VALIDATEOPS_ON;
            cout<<"VALIDATION ENABLED for every operation: VALIDATEOPS state is "<<___validateops<<endl;
        } else {
            cout<<"bad arguments"<<endl;
            exit(1);
        }
    }
    TOTAL_THREADS = WORK_THREADS + RQ_THREADS;
    
//    PRINTS(P1NAME);
//    PRINTS(P2NAME);
//    PRINTS(P3NAME);
    PRINTS(STR(FIND_AND_CHECK_SUCCESS));
    PRINTS(STR(INSERT_AND_CHECK_SUCCESS));
    PRINTS(STR(DELETE_AND_CHECK_SUCCESS));
//    PRINTS(STR(FIND_FUNC));
//    PRINTS(STR(INSERT_FUNC));
//    PRINTS(STR(ERASE_FUNC));
//    PRINTS(STR(RQ_FUNC));
    PRINTS(STR(EXPERIMENT_FN));
//    PRINTI(MAX_FAST_HTM_RETRIES);
//    PRINTI(MAX_SLOW_HTM_RETRIES);
    PRINTI(PREFILL);
    PRINTI(MILLIS_TO_RUN);
    PRINTI(INS);
    PRINTI(DEL);
    PRINTI(MAXKEY);
    PRINTI(WORK_THREADS);
//    PRINTI(RQ_THREADS);
    PRINTS(RECLAIM_TYPE);
    PRINTS(ALLOC_TYPE);
    PRINTS(POOL_TYPE);
    PRINTI(PRINT_TREE);
    
    keysum = new debugCounter(TOTAL_THREADS);
    
    performExperiment();
    
    delete keysum;
    return 0;
}
