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
#include <cassert>
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
#include "../hytm1/platform.h" // for SYNC_RMW primitive

static Random rngs[MAX_TID_POW2*PREFETCH_SIZE_WORDS]; // create per-thread random number generators (padded to avoid false sharing)

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

#define STR(x) XSTR(x)
#define XSTR(x) #x

//debugCounter successful (PHYSICAL_PROCESSORS);

volatile char padding0[512];
volatile long long data;
volatile char padding1[512];

#define INIT_THREAD(tid) 
#define PRCU_INIT 
#define PRCU_REGISTER(tid) 
#define PRCU_UNREGISTER
#define CLEAR_COUNTERS 
#define INCREMENT(name) 
#define ADD(name, val) 
#define GET_TOTAL(name) 

void thread_timed(void *unused) {
    int tid = thread_getId();
    binding_bindThread(tid, PHYSICAL_PROCESSORS);
    SYNC;
//    TM_THREAD_ENTER();
    PRCU_REGISTER(tid);
    Random *rng = &rngs[tid*PREFETCH_SIZE_WORDS];

    VERBOSE COUTATOMICTID("timed thread initializing"<<endl);
    INIT_THREAD(tid);
    VERBOSE COUTATOMICTID("join running"<<endl);
    running.fetch_add(1);
    SYNC;
    int cnt = 0;
    
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
                SYNC;
                break;
            }
//                VERBOSE COUTATOMICTID("not stopping yet: duration="<<duration<<" MILLIS_TO_RUN="<<MILLIS_TO_RUN<<endl);
        }

        __sync_fetch_and_add(&data, 1);
    }
    
    running.fetch_add(-1);
    SYNC_RMW;
    VERBOSE COUTATOMICTID("termination"<<endl);
    
    PRCU_UNREGISTER;
    VERBOSE COUTATOMICTID("calling TM_THREAD_EXIT"<<endl);
//    TM_THREAD_EXIT();
}

void trial() {
    data = 0;
    
    // get random number generator seeded with time
    // we use this rng to seed per-thread rng's that use a different algorithm
    srand(time(NULL));
    for (int i=0;i<PHYSICAL_PROCESSORS /*TOTAL_THREADS*/;++i) {
        rngs[i*PREFETCH_SIZE_WORDS].setSeed(rand());
    }
    
//    TM_STARTUP(TOTAL_THREADS);
    SYNC;
    
    thread_startup(TOTAL_THREADS);
    
    startTime = chrono::high_resolution_clock::now();
    SYNC;
    
    thread_start(thread_timed, NULL);
    
    endTime = chrono::high_resolution_clock::now();
    SYNC;
    elapsedMillis = chrono::duration_cast<chrono::milliseconds>(endTime-startTime).count();
    COUTATOMIC((elapsedMillis/1000.)<<"s"<<endl);
    
    thread_shutdown();
//    TM_SHUTDOWN();
}

#define PRINTI(name) { cout<<(#name)<<"="<<name<<endl; }
#define PRINTS(name) { cout<<(#name)<<"="<<name<<endl; }

int main(int argc, char** argv) {
    MILLIS_TO_RUN = 1000;
    WORK_THREADS = 0;

    for (int i=1;i<argc;++i) {
        if (strcmp(argv[i], "-n") == 0) {
            WORK_THREADS = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0) {
            MILLIS_TO_RUN = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-bind") == 0) { // e.g., "-bind 1,2,3,8-11,4-7,0"
            binding_parseCustom(argv[++i]); // e.g., "1,2,3,8-11,4-7,0"
        } else {
            cout<<"bad arguments"<<endl;
            exit(1);
        }
    }
    
    binding_configurePolicy(PHYSICAL_PROCESSORS);

    TOTAL_THREADS = WORK_THREADS;
    PRINTI(TOTAL_THREADS);
    PRINTI(MILLIS_TO_RUN);
    PRINTI(THREAD_BINDING);
    
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
        
    trial();
    SYNC;
    cout<<"performed "<<data<<" successful increments in 1 second"<<endl;
    
    return 0;
}
