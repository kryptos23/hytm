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
#include "dsbench_tm.h"
#include "common/random.h"
#include "globals.h"
#include "globals_extern.h"
#include "recordmgr/machineconstants.h"
#include "binding.h"
//#include "../hytm1/platform_impl.h" // for SYNC_RMW primitive

#ifndef EXPERIMENT_FN
#define EXPERIMENT_FN trial
#endif

#define DEFAULT_SUSPECTED_SIGNAL SIGQUIT
static Random rngs[MAX_TID_POW2*PREFETCH_SIZE_WORDS]; // create per-thread random number generators (padded to avoid false sharing)

#if defined(BST)
#include "bst/bst_impl.h"
#include "recordmgr/record_manager.h"
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
// note: tree->debugGetCounters()[tid].keysum contains the key sum hash for thread tid (including for prefilling)

const int OPS_BETWEEN_TIME_CHECKS = 500;
const int OPS_BETWEEN_TIME_CHECKS_RQ = 10;

// note: tree->debugGetCounters()[tid].prefillSize contains the number of keys added to the prefilled tree by thread tid
const long long PREFILL_INTERVAL_MILLIS = 200;

// create a binary search tree with an allocator that uses a new form of epoch based memory reclamation
const test_type NO_KEY = -1;
const test_type NO_VALUE = -1;
const int RETRY = -2;
void *__tree;

const test_type NEG_INFTY = -1;
const test_type POS_INFTY = 2000000000;

#if defined(BST)
#define DS_DECLARATION bst<test_type, test_type, less<test_type>, MemMgmt>
#else
#error "Failed to define a data structure"
#endif

#define STR(x) XSTR(x)
#define XSTR(x) #x

#define VALUE key
#define KEY key

#if defined(BST)
    #if defined(TLE) || defined(tle)
        #define INSERT_AND_CHECK_SUCCESS tree->insert_tle(tid, key, VALUE) == tree->NO_VALUE
        #define DELETE_AND_CHECK_SUCCESS tree->erase_tle(tid, key).second
        #define FIND_AND_CHECK_SUCCESS tree->find_tle(tid, key)
        #define RQ_AND_CHECK_SUCCESS(rqcnt) { cout<<"ERROR: RQ not supported in TLE mode"<<endl; exit(-1); }
    #else
        #define INSERT_AND_CHECK_SUCCESS tree->insert_stm(TM_ARG_ALONE, tid, key, VALUE) == tree->NO_VALUE
        #define DELETE_AND_CHECK_SUCCESS tree->erase_stm(TM_ARG_ALONE, tid, key).second
        #define FIND_AND_CHECK_SUCCESS tree->find_stm(TM_ARG_ALONE, tid, key)
        #define RQ_AND_CHECK_SUCCESS(rqcnt) (rqcnt) = tree->rangeUpdate_stm(TM_ARG_ALONE, tid, key, key+RQSIZE)
    #endif

    #define INIT_THREAD(tid) tree->initThread(tid)
    #define PRCU_INIT 
    #define PRCU_REGISTER(tid)
    #define PRCU_UNREGISTER
    #define CLEAR_COUNTERS tree->clearCounters();
    #define INCREMENT(name) (++tree->debugGetCounters()[tid]->name)
    #define ADD(name, val) (tree->debugGetCounters()[tid]->name += (val))
    #define GET_TOTAL(name) ({ long long ___totalsum=0; for (int ___totali=0;___totali<TOTAL_THREADS;++___totali) { ___totalsum += tree->debugGetCounters()[___totali]->name; } ___totalsum; })
#endif

template <class MemMgmt>
void thread_prefill(void *unused) {
    int tid = thread_getId();
    TRACE COUTATOMICTID("prefill thread started"<<endl);
    binding_bindThread(tid, PHYSICAL_PROCESSORS);
    TM_THREAD_ENTER();
    PRCU_REGISTER(tid);
    Random *rng = &rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * tree = (DS_DECLARATION *) __tree;

    double insProbability = (INS > 0 ? 100*INS/(INS+DEL) : 50.);
    
    INIT_THREAD(tid);
    running.fetch_add(1);
    __sync_synchronize();
    while (!start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    int cnt = 0;
    while (!done) {
//        TRACE COUTATOMICTID("prefill thread cnt="<<cnt<<endl);
        if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
            chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
            chrono::time_point<chrono::high_resolution_clock> __startTime = startTime;
            auto duration = chrono::duration_cast<chrono::milliseconds>(__endTime-__startTime).count();
            if ((int) duration >= PREFILL_INTERVAL_MILLIS) {
                done = true;
                __sync_synchronize();
                break;
            }
        }

        VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
        int key = rng->nextNatural(MAXKEY);
        double op = rng->nextNatural(100000000) / 1000000.;
        if (op < insProbability) {
            if (INSERT_AND_CHECK_SUCCESS) {
                ADD(keysum, key);
                ADD(prefillSize, 1);
            }
        } else {
            if (DELETE_AND_CHECK_SUCCESS) {
                ADD(keysum, -key);
                ADD(prefillSize, -1);
            }
        }
    }
    running.fetch_add(-1);
    SYNC_RMW;
    PRCU_UNREGISTER;
    TM_THREAD_EXIT();
}

template <class MemMgmt>
void prefill(DS_DECLARATION * tree) {
    chrono::time_point<chrono::high_resolution_clock> prefillStartTime = chrono::high_resolution_clock::now();
    __sync_synchronize();

    const double PREFILL_THRESHOLD = 0.05;
    const int MAX_ATTEMPTS = 500;
    const double expectedFullness = (INS+DEL ? INS / (double)(INS+DEL) : 0.5); // percent full in expectation
    const int expectedSize = (int)(MAXKEY * expectedFullness);

    int sz = 0;
    int attempts;
    TRACE COUTATOMIC("main thread: starting tm"<<endl);
//    TM_STARTUP(TOTAL_THREADS);
    for (attempts=0;attempts<MAX_ATTEMPTS;++attempts) {
//        COUTATOMIC("main thread: prefilling iteration "<<attempts<<endl);
        TRACE COUTATOMIC("main thread: creating prefill threads"<<endl);
        thread_startup(TOTAL_THREADS);

        startTime = chrono::high_resolution_clock::now();
        __sync_synchronize();

        TRACE COUTATOMIC("main thread: starting prefilling threads"<<endl);
        start = true;
        thread_start(thread_prefill<MemMgmt>, NULL);

        TRACE COUTATOMIC("main thread: shutting down threads"<<endl);
        thread_shutdown();
        
        start = false;
        done = false;
        SYNC_RMW;

        sz = GET_TOTAL(prefillSize);
        //sz = tree->getSize();
        int absdiff = (sz < expectedSize ? expectedSize - sz : sz - expectedSize);
        if (absdiff < expectedSize*PREFILL_THRESHOLD) {
            break;
        }
        TRACE COUTATOMIC("main thread: prefilling iteration "<<attempts<<" sz="<<sz<<" expectedSize="<<expectedSize<<" keysum="<<GET_TOTAL(keysum)<<" treekeysum="<<tree->debugKeySum()<<" treesize="<<tree->size()<<endl);
    }
    TRACE COUTATOMIC("main thread: shutting down tm"<<endl);
//    TM_SHUTDOWN();
    
    if (attempts >= MAX_ATTEMPTS) {
        cerr<<"ERROR: could not prefill to expected size "<<expectedSize<<". reached size "<<sz<<" after "<<attempts<<" attempts"<<endl;
        exit(-1);
    }
    
    /**
     * clear counters, but preserve the keysum,
     * so we can compare the keysum in the final tree
     * (which may include keys only touched during prefilling)
     * with the keysum for all threads
     */
    long long keysum = GET_TOTAL(keysum);
    CLEAR_COUNTERS;
    int tid = 0;
    ADD(keysum, keysum);
    
    chrono::time_point<chrono::high_resolution_clock> prefillEndTime = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::milliseconds>(prefillEndTime-prefillStartTime).count();
    COUTATOMIC(endl);
    COUTATOMIC("###############################"<<endl);
    COUTATOMIC("##### FINISHED PREFILLING ##### (running="<<running.load()<<" size "<<sz<<" for expected size "<<expectedSize<<" keysum="<<GET_TOTAL(keysum)<<" treekeysum="<<tree->debugKeySum()<<" treesize="<<tree->size()<<" in "<<(elapsed/1000.)<<"s)"<<endl);
    COUTATOMIC("###############################"<<endl);
    COUTATOMIC(endl);
    __sync_synchronize();
    if (GET_TOTAL(keysum) != tree->debugKeySum()) {
        COUTATOMIC("ERROR: validation FAILED"<<endl);
        exit(-1);
    }
}

template <class MemMgmt>
void thread_timed(void *unused) {
    int tid = thread_getId();
    binding_bindThread(tid, PHYSICAL_PROCESSORS);
    __sync_synchronize();
    TM_THREAD_ENTER();
    PRCU_REGISTER(tid);
    Random *rng = &rngs[tid*PREFETCH_SIZE_WORDS];
    DS_DECLARATION * tree = (DS_DECLARATION *) __tree;

#if defined(BST)
    //Node<test_type, test_type> const ** rqResults = new Node<test_type, test_type> const *[RQSIZE];
    Node<test_type, test_type> const rqResults[RQSIZE];
#endif
    
    VERBOSE COUTATOMICTID("timed thread initializing"<<endl);
    INIT_THREAD(tid);
    VERBOSE COUTATOMICTID("join running"<<endl);
    running.fetch_add(1);
    __sync_synchronize();
//    while (!start) { __sync_synchronize(); TRACE COUTATOMICTID("waiting to start"<<endl); } // wait to start
    int cnt = 0;
    
    if (tid >= WORK_THREADS) {
        VERBOSE COUTATOMICTID("identifying as an rq thread"<<endl);
        // rq thread
        while (!done) {
//            VERBOSE COUTATOMICTID("rq thread cnt="<<cnt<<endl);
            if (((++cnt) % OPS_BETWEEN_TIME_CHECKS_RQ) == 0) {
                chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
                chrono::time_point<chrono::high_resolution_clock> __startTime = startTime;
                auto duration = chrono::duration_cast<chrono::milliseconds>(__endTime-__startTime).count();
                if ((int) duration >= MILLIS_TO_RUN) {
                    done = true;
                    __sync_synchronize();
                    break;
                }
//                VERBOSE COUTATOMICTID("not stopping yet: duration="<<duration<<" MILLIS_TO_RUN="<<MILLIS_TO_RUN<<endl);
            }

            VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
            int key = rng->nextNatural(MAXKEY);
            //cout<<"MAXKEY="<<MAXKEY<<" random key for tid="<<tid<<" is "<<key<<endl;
//            double op = rng->nextNatural(100000000) / 1000000.;
//            if (op < INS) {
//                if (INSERT_AND_CHECK_SUCCESS) {
//                    ADD(keysum, key);
//                }
//                INCREMENT(insertSuccess);
//            } else if (op < INS+DEL) {
//                if (DELETE_AND_CHECK_SUCCESS) {
//                    ADD(keysum, -key);
//                }
//                INCREMENT(eraseSuccess);
//            } else if (op < INS+DEL+RQ) {
                int rqcnt;
                RQ_AND_CHECK_SUCCESS(rqcnt);
                INCREMENT(rqSuccess);
//            } else {
//                FIND_AND_CHECK_SUCCESS;
//                INCREMENT(findSuccess);
//            }
        }
    } else {
        VERBOSE COUTATOMICTID("identifying as a work thread"<<endl);
        // work thread
        while (!done) {
//            VERBOSE COUTATOMICTID("work thread cnt="<<cnt<<endl);
            if (((++cnt) % OPS_BETWEEN_TIME_CHECKS) == 0) {
                chrono::time_point<chrono::high_resolution_clock> __endTime = chrono::high_resolution_clock::now();
                chrono::time_point<chrono::high_resolution_clock> __startTime = startTime;
                auto duration = chrono::duration_cast<chrono::milliseconds>(__endTime-__startTime).count();
                if ((int) duration >= MILLIS_TO_RUN) {
                    done = true;
                    __sync_synchronize();
                    break;
                }
//                VERBOSE COUTATOMICTID("not stopping yet: duration="<<duration<<" MILLIS_TO_RUN="<<MILLIS_TO_RUN<<endl);
            }

            VERBOSE if (cnt&&((cnt % 1000000) == 0)) COUTATOMICTID("op# "<<cnt<<endl);
            int key = rng->nextNatural(MAXKEY);
            //cout<<"MAXKEY="<<MAXKEY<<" random key for tid="<<tid<<" is "<<key<<endl;
            double op = rng->nextNatural(100000000) / 1000000.;
            if (op < INS) {
                if (INSERT_AND_CHECK_SUCCESS) {
                    ADD(keysum, key);
                }
                INCREMENT(insertSuccess);
            } else if (op < INS+DEL) {
                if (DELETE_AND_CHECK_SUCCESS) {
                    ADD(keysum, -key);
                }
                INCREMENT(eraseSuccess);
            } else if (op < INS+DEL+RQ) {
                int rqcnt;
                RQ_AND_CHECK_SUCCESS(rqcnt);
                INCREMENT(rqSuccess);
            } else {
                FIND_AND_CHECK_SUCCESS;
                INCREMENT(findSuccess);
            }
        }
    }
    
    running.fetch_add(-1);
    SYNC_RMW;
    VERBOSE COUTATOMICTID("termination"<<endl);
    
    PRCU_UNREGISTER;
    VERBOSE COUTATOMICTID("calling TM_THREAD_EXIT"<<endl);
    TM_THREAD_EXIT();
}

template <class MemMgmt>
void trial() {
#if defined(BST)
    __tree = (void*) new DS_DECLARATION(NO_KEY, NO_VALUE, RETRY, PHYSICAL_PROCESSORS /*TOTAL_THREADS*/);
#else
#error "Failed to define a data structure"
#endif
    
    // get random number generator seeded with time
    // we use this rng to seed per-thread rng's that use a different algorithm
    srand(time(NULL));
    for (int i=0;i<PHYSICAL_PROCESSORS /*TOTAL_THREADS*/;++i) {
        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
    }
    
    TM_STARTUP(TOTAL_THREADS);
    if (PREFILL) prefill((DS_DECLARATION *) __tree);
    __sync_synchronize();
    
    thread_startup(TOTAL_THREADS);
    
    startTime = chrono::high_resolution_clock::now();
    __sync_synchronize();
    
    thread_start(thread_timed<MemMgmt>, NULL);
    
    endTime = chrono::high_resolution_clock::now();
    __sync_synchronize();
    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
    
    thread_shutdown();
    TM_SHUTDOWN();
    SYNC_RMW;
}

#ifdef BST
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
    //debugCounters& counters = tree->debugGetCounters();

    long long treeKeySum = tree->debugKeySum();
    long long threadsKeySum = GET_TOTAL(keysum);
    if (threadsKeySum == treeKeySum) {
        cout<<"Validation OK: threadsKeySum="<<threadsKeySum<<" treeKeySum="<<treeKeySum<<endl;
    } else {
        cout<<"Validation FAILURE: threadsKeySum="<<threadsKeySum<<" treeKeySum="<<treeKeySum<<endl;
#if defined(BST)
        tree->debugPrintToFile("tree_", 0, "", 0, ".out");
#endif
        exit(-1);
    }

//    int totalSuccessful = GET_TOTAL(pathSuccess);
//    int totalChanges = GET_TOTAL(updateChange);
//    int totalOps = totalSuccessful + totalChanges;

    COUTATOMIC(endl);
    
    long long rqSuccessTotal = GET_TOTAL(rqSuccess);
    long long rqFailTotal = GET_TOTAL(rqFail);
    long long findSuccessTotal = GET_TOTAL(findSuccess);
    long long findFailTotal = GET_TOTAL(findFail);
//    for (int path=0;path<NUMBER_OF_PATHS;++path) {
//        rqSuccessTotal += GET_TOTAL(rqSuccess);
//        rqFailTotal += GET_TOTAL(rqFail);
//        findSuccessTotal += GET_TOTAL(findSuccess);
//        findFailTotal += GET_TOTAL(findFail);
//    }
    if (GET_TOTAL(insertSuccess)) COUTATOMIC("total insert succ             : "<<GET_TOTAL(insertSuccess)<<endl);
    if (GET_TOTAL(insertFail)) COUTATOMIC("total insert retry            : "<<GET_TOTAL(insertFail)<<endl);
    if (GET_TOTAL(eraseSuccess)) COUTATOMIC("total erase succ              : "<<GET_TOTAL(eraseSuccess)<<endl);
    if (GET_TOTAL(eraseFail)) COUTATOMIC("total erase retry             : "<<GET_TOTAL(eraseFail)<<endl);
    if (findSuccessTotal) COUTATOMIC("total find succ               : "<<findSuccessTotal<<endl);
    if (findFailTotal) COUTATOMIC("total find retry              : "<<findFailTotal<<endl);
    if (rqSuccessTotal) COUTATOMIC("total rq succ                 : "<<rqSuccessTotal<<endl);
    if (rqFailTotal) COUTATOMIC("total rq fail                 : "<<rqFailTotal<<endl);
    const long totalSuccUpdates = GET_TOTAL(insertSuccess)+GET_TOTAL(eraseSuccess);
    const long totalSuccAll = totalSuccUpdates + rqSuccessTotal + findSuccessTotal;
    const long throughput = (long) (totalSuccUpdates / (MILLIS_TO_RUN/1000.));
    const long throughputAll = (long) (totalSuccAll / (MILLIS_TO_RUN/1000.));
    COUTATOMIC("total succ updates            : "<<totalSuccUpdates<<endl);
    COUTATOMIC("total succ                    : "<<totalSuccAll<<endl);
    COUTATOMIC("throughput (succ updates/sec) : "<<throughput<<endl);
    COUTATOMIC("    incl. queries             : "<<throughputAll<<endl);
    COUTATOMIC("elapsed milliseconds          : "<<MILLIS_TO_RUN<<endl);
    COUTATOMIC(endl);

    COUTATOMIC(endl);
   
#if defined(BST)
    if (PRINT_TREE) {
        tree->debugPrintToFile("tree_", 0, "", 0, ".out");
    }
    
    // free tree
    delete tree;
#endif
}

#define PRINTI(name) { cout<<(#name)<<"="<<name<<endl; }
#define PRINTS(name) { cout<<(#name)<<"="<<name<<endl; }

template <class Reclaim, class Alloc, class Pool>
void performExperiment() {
#if defined(BST)
    typedef record_manager<Reclaim, Alloc, Pool, Node<test_type, test_type>, SCXRecord<test_type, test_type> > MemMgmt;
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
        } else if (strcmp(argv[i], "-nrq") == 0) {
            RQ_THREADS = atoi(argv[++i]);
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
        } else if (strcmp(argv[i], "-bind") == 0) { // e.g., "-bind 1,2,3,8-11,4-7,0"
            binding_parseCustom(argv[++i]); // e.g., "1,2,3,8-11,4-7,0"
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
    binding_configurePolicy(PHYSICAL_PROCESSORS);

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
    PRINTI(HTM_ATTEMPT_THRESH);
    PRINTI(PREFILL);
    PRINTI(MILLIS_TO_RUN);
    PRINTI(INS);
    PRINTI(DEL);
    PRINTI(MAXKEY);
    PRINTI(WORK_THREADS);
    PRINTI(RQ_THREADS);
    PRINTS(RECLAIM_TYPE);
    PRINTS(ALLOC_TYPE);
    PRINTS(POOL_TYPE);
    PRINTI(PRINT_TREE);
    PRINTI(THREAD_BINDING);
    
#ifdef RECORD_ABORT_ADDRESSES
    cout<<"RECORD_ABORT_ADDRESSES=1"<<endl;
#else
    cout<<"RECORD_ABORT_ADDRESSES=0"<<endl;
#endif
    
    // print actual thread bindings
    cout<<"ACTUAL_THREAD_BINDINGS=";
    for (int i=0;i<TOTAL_THREADS;++i) {
        cout<<(i?",":"")<<binding_getActualBinding(i, PHYSICAL_PROCESSORS);
    }
    cout<<endl;
    
    if (!binding_isInjectiveMapping(TOTAL_THREADS, PHYSICAL_PROCESSORS)) {
        cout<<"ERROR: thread binding maps more than one thread to a single logical processor"<<endl;
        exit(-1);
    }
        
    performExperiment();
    
    return 0;
}
