/* 
 * File:   binding.h
 * Author: tabrown
 *
 * Created on June 23, 2016, 6:52 PM
 * 
 * Used to configure and implement a thread binding/pinning policy.
 * 
 * Instructions:
 * 1. invoke binding_configurePolicy, passing the number of logical processors.
 * 2. either #define one of the binding policies below, OR
 *    invoke binding_parseCustom, passing a string that describes the desired
 *    thread binding policy, e.g., "1,2,3,8-11,4-7,0".
 *    the string contains the ids of logical processors, or ranges of ids,
 *    separated by commas.
 * 3. have each thread invoke binding_bindThread.
 * 4. after your experiments run, you can confirm the binding for a given thread
 *    by invoking binding_getActualBinding.
 *    you can also check whether all logical processors had at most one thread
 *    mapped to them by invoking binding_isInjectiveMapping.
 */

#ifndef BINDING_H
#define	BINDING_H

#include <cassert>
#include <sched.h>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "recordmgr/machineconstants.h"
using namespace std;

const int NONE = 0;
const int IDENTITY = 1;
const int NEELAM_SCATTER = 2;              // specific to oracle x5-2
const int NEELAM_SOCKET1_THEN_SOCKET2 = 3; // specific to oracle x5-2
const int POMELA6_SCATTER = 4;
const int TAPUZ40_SCATTER = 5;
const int TAPUZ40_CLUSTER = 6;
const int ZYRA_SCATTER = 7;
const int ZYRA_CLUSTER = 8;
const int JAX_SCATTER = 9;
const int JAX_CLUSTER = 10;
const int SOSCIP_CLUSTER = 100;
const int SOSCIP_CLUSTER48 = 101;
const int SOSCIP_SCATTER = 102;

PAD;

// cpu sets for binding threads to cores
static cpu_set_t *cpusets[PHYSICAL_PROCESSORS];

static int customBinding[PHYSICAL_PROCESSORS];
static int numCustomBindings = 0;
//static vector<int> customBinding;

PAD;

static unsigned digits(unsigned x) {
    int d = 1;
    while (x > 9) {
        x /= 10;
        ++d;
    }
    return d;
}

// parse token starting at argv[ix],
// place bindings for the token at the end of customBinding, and
// return the index of the first character in the next token,
//     or the size of the string argv if there are no further tokens.
static unsigned parseToken(string argv, int ix) {
    // token is of one of following forms:
    //      INT
    //      INT-INT
    // and is either followed by "," is the end of the string.
    // we first determine which is the case
    
    // read first INT
    int ix2 = ix;
    while (ix2 < argv.size() && argv[ix2] != ',') ++ix2;
    string token = argv.substr(ix, ix2-ix+1);
    int a = atoi(token.c_str());
    
    // check if the token is of the first form: INT
    ix = ix+digits(a);              // first character AFTER first INT
    if (ix >= argv.size() || argv[ix] == ',') {
        
        // add single binding
        //cout<<"a="<<a<<endl;
        //customBinding.push_back(a);
        customBinding[numCustomBindings++] = a;
    
    // token is of the second form: INT-INT
    } else {
        assert(argv[ix] == '-');
        ++ix;                       // skip '-'
        
        // read second INT
        token = argv.substr(ix, ix2-ix+1);
        int b = atoi(token.c_str());
        //cout<<"a="<<a<<" b="<<b<<endl;
        
        // add range of bindings
        for (int i=a;i<=b;++i) {
            //customBinding.push_back(i);
            customBinding[numCustomBindings++] = i;
        }
        
        ix = ix+digits(b);          // first character AFTER second INT
    }
    // note: ix is the first character AFTER the last INT in the token
    // this is either a comma (',') or the end of the string argv.
    return (ix >= argv.size() ? argv.size() : ix+1 /* skip ',' */);
}

// argv contains a custom thread binding pattern, e.g., "1,2,3,8-11,4-7,0"
// threads will be bound according to this binding
void binding_parseCustom(string argv) {
    //customBinding.clear();
    numCustomBindings = 0;
    
    unsigned ix = 0;
    while (ix < argv.size()) {
        ix = parseToken(argv, ix);
    }
//    cout<<"custom thread binding :";
//    for (int i=0;i<customBinding.size();++i) {
//        cout<<" "<<customBinding[i];
//    }
//    cout<<endl;
}

static void doBindThread(const int tid, const int nprocessors) {
    if (sched_setaffinity(0, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) { // bind thread to core
        cout<<"ERROR: could not bind thread "<<tid<<" to cpuset "<<cpusets[tid%nprocessors]<<endl;
        exit(-1);
    }
//    for (int i=0;i<nprocessors;++i) {
//        if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) {
//            //COUTATOMICTID("binding thread "<<tid<<" to cpu "<<i<<endl);
//        } else {
//            COUTATOMICTID("ERROR binding to cpu "<<i<<endl);
//            exit(-1);
//        }
//    }
}

int binding_getActualBinding(const int tid, const int nprocessors) {
    int result = -1;
#ifndef THREAD_BINDING
    if (numCustomBindings == 0) {
        return result;
    }
#endif
    unsigned bindings = 0;
    for (int i=0;i<nprocessors;++i) {
        if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) {
            result = i;
            ++bindings;
        }
    }
    if (bindings > 1) {
        cout<<"ERROR: "<<bindings<<" processor bindings for thread "<<tid<<endl;
        exit(-1);
    }
    return result;
}

bool binding_isInjectiveMapping(const int nthreads, const int nprocessors) {
#ifndef THREAD_BINDING
    if (numCustomBindings == 0) {
        return true;
    }
#endif
    bool covered[nprocessors];
    for (int i=0;i<nprocessors;++i) covered[i] = 0;
    for (int i=0;i<nthreads;++i) {
        int ix = binding_getActualBinding(i, nprocessors);
        if (covered[ix]) return false;
        covered[ix] = 1;
    }
    return true;
}

void binding_bindThread(const int tid, const int nprocessors) {
    //if (customBinding.empty()) {
    if (numCustomBindings == 0) {
#ifdef THREAD_BINDING
        if (THREAD_BINDING != NONE) {
            doBindThread(tid, nprocessors);
        }
#endif
    } else {
        doBindThread(tid, nprocessors);
    }
}

void binding_configurePolicy(const int nprocessors) {
    //if (customBinding.empty()) {
    if (numCustomBindings == 0) {
#ifdef THREAD_BINDING
        // create cpu sets for binding threads to cores
        int size = CPU_ALLOC_SIZE(nprocessors);
        for (int i=0;i<nprocessors;++i) {
            cpusets[i] = CPU_ALLOC(nprocessors);
            CPU_ZERO_S(size, cpusets[i]);
            int j = -1;
            switch (THREAD_BINDING) {
                case IDENTITY:
                    j = i;
                    break;
                case NEELAM_SCATTER:
                    if (i >= nprocessors / 2) { // hyperthreading
                        j = i - nprocessors / 2;
                    } else {
                        j = i;
                    }
                    if (i & 1) j++;
                    j /= 2;
                    if (i & 1) j--;
                    if (i & 1) { // odd
                        j += nprocessors / 4;
                    }
                    if (i >= nprocessors / 2) { // hyperthreading
                        j += nprocessors / 2;
                    }
                    break;
                case NEELAM_SOCKET1_THEN_SOCKET2:
                    j = i;
                    if (j >= nprocessors / 4 && j < nprocessors / 2) {
                        j += 18;
                    } else if (j >= nprocessors / 2 && j < 3 * nprocessors / 4) {
                        j -= 18;
                    }
                    break;
                case POMELA6_SCATTER:
                    j = (i%4)*16+(i/4); // over 4 sockets
                    //j = (i%8)*8+(i/8); // over 8 numa nodes
                    break;
                case TAPUZ40_SCATTER:
                    j = (i%2)*12+(i/2)+(i/24)*12;
                    break;
                case TAPUZ40_CLUSTER:
                    j = (i<12?i:(i<24?i+12:(i<36?i-12:i)));
                    break;
                case ZYRA_SCATTER:
                    // lscpu:
                    //  NUMA node0 CPU(s):   0-17,72-89
                    //  NUMA node1 CPU(s):   18-35,90-107
                    //  NUMA node2 CPU(s):   36-53,108-125
                    //  NUMA node3 CPU(s):   54-71,126-143
                    //
                    // command to visualize:
                    //  lscpu | grep "NUMA node[0-9]" ; for i in {0..143} ; do if (( (i%4) == 0 )) ; then echo ; fi ; echo -n $(( (i%4)*18 + i/4 + (i/(18*4))*(18*(4-1)) )) "" ; done ; echo
                    //
                    j = (i%4)*18 + i/4 + (i/(18*4))*(18*(4-1));
                    break;
                case ZYRA_CLUSTER:
                    // lscpu:
                    //  NUMA node0 CPU(s):   0-17,72-89
                    //  NUMA node1 CPU(s):   18-35,90-107
                    //  NUMA node2 CPU(s):   36-53,108-125
                    //  NUMA node3 CPU(s):   54-71,126-143
                    //
                    // command to visualize:
                    //  lscpu | grep "NUMA node[0-9]" ; for i in {0..143} ; do if (( (i%18) == 0 )) ; then echo ; fi ; echo -n $(( ((i%36)>=18)*(144/2) + i%18 + (i/36)*18 )) "" ; done ; echo
                    //
                    j = ((i%36)>=18)*(144 / 2/*HTs*/) + i%18 + (i/36)*18;
                    break;
                case JAX_SCATTER:
                    // lscpu:
                    // NUMA node0 CPU(s):   0-23,96-119
                    // NUMA node1 CPU(s):   24-47,120-143
                    // NUMA node2 CPU(s):   48-71,144-167
                    // NUMA node3 CPU(s):   72-95,168-191
                    // command to visualize:
                    //  lscpu | grep "NUMA node[0-9]" ; for i in {0..191} ; do if (( (i%4) == 0 )) ; then echo ; fi ; echo -n $(( (i%4)*24 + i/4 + (i/(24*4))*(24*(4-1)) )) "" ; done ; echo
                    //
                    j = (i%4)*24 + i/4 + (i/(24*4))*(24*(4-1));
                    break;
                case JAX_CLUSTER:
                    // lscpu:
                    // NUMA node0 CPU(s):   0-23,96-119
                    // NUMA node1 CPU(s):   24-47,120-143
                    // NUMA node2 CPU(s):   48-71,144-167
                    // NUMA node3 CPU(s):   72-95,168-191
                    //
                    // command to visualize:
                    //  lscpu | grep "NUMA node[0-9]" ; for i in {0..191} ; do if (( (i%24) == 0 )) ; then echo ; fi ; echo -n $(( ((i%48)>=24)*(192/2) + i%24 + (i/48)*24 )) "" ; done ; echo
                    //
                    j = ((i%48)>=24)*(192 / 2/*HTs*/) + i%24 + (i/48)*24;
                    break;
                case SOSCIP_CLUSTER:
    #define SOSCIP_CLUSTER_coresPerSocket 12
    #define SOSCIP_CLUSTER_threadsPerCore 8
    #define SOSCIP_CLUSTER_threadsPerSocket ((SOSCIP_CLUSTER_coresPerSocket)*(SOSCIP_CLUSTER_threadsPerCore))
                    j = (i%SOSCIP_CLUSTER_coresPerSocket)*SOSCIP_CLUSTER_threadsPerCore
                            + (i%SOSCIP_CLUSTER_threadsPerSocket)/SOSCIP_CLUSTER_coresPerSocket
                            + (i/SOSCIP_CLUSTER_threadsPerSocket)*SOSCIP_CLUSTER_threadsPerSocket;
                    break;
                case SOSCIP_CLUSTER48:
    #define SOSCIP_CLUSTER48_coresPerSocket 12
    #define SOSCIP_CLUSTER48_threadsPerCore 2
    #define SOSCIP_CLUSTER48_threadsPerSocket ((SOSCIP_CLUSTER48_coresPerSocket)*(SOSCIP_CLUSTER48_threadsPerCore))
                    j = (i%SOSCIP_CLUSTER48_coresPerSocket)*SOSCIP_CLUSTER48_threadsPerCore
                            + (i%SOSCIP_CLUSTER48_threadsPerSocket)/SOSCIP_CLUSTER48_coresPerSocket
                            + (i/SOSCIP_CLUSTER48_threadsPerSocket)*SOSCIP_CLUSTER48_threadsPerSocket;
                    break;
                case SOSCIP_SCATTER:
                    // output = MOD(A1,J$5*J$4)*J$6+INT(A1/J$5/J$4) where a1=i, j4=sockets, j5=cores/socket, j6=threads/core
                    j = (i%24)*8 + i/24;
                    //j = (i%(2*SOSCIP_CLUSTER_coresPerSocket))*SOSCIP_CLUSTER_threadsPerCore + (i/SOSCIP_CLUSTER_coresPerSocket/2);
                    break;
                case NONE:
                    break;
            }
            if (THREAD_BINDING != NONE) {
                CPU_SET_S(j, size, cpusets[i]);
            }
        }
#endif
    } else {
        // create cpu sets for binding threads to cores
//        bool warning = false;
        int size = CPU_ALLOC_SIZE(nprocessors);
        for (int i=0;i<nprocessors;++i) {
            cpusets[i] = CPU_ALLOC(nprocessors);
            CPU_ZERO_S(size, cpusets[i]);
            //CPU_SET_S(customBinding[i%customBinding.size()], size, cpusets[i]);
            CPU_SET_S(customBinding[i%numCustomBindings], size, cpusets[i]);

//            if (i < customBinding.size()) {
//                //cout<<"setting up thread binding for thread "<<i<<" to processor "<<customBinding[i]<<endl;
//                CPU_SET_S(customBinding[i], size, cpusets[i]);
//            } else {
//                warning = true;
//            }
        }
//        if (warning) {
//            cout<<"WARNING: "<<nprocessors<<" threads mapped to "<<customBinding.size()<<" processors"<<endl;
//        }
    }
}

#endif	/* BINDING_H */

