/* 
 * File:   binding.h
 * Author: tabrown
 *
 * Created on June 23, 2016, 6:52 PM
 */

#ifndef BINDING_H
#define	BINDING_H

#include <sched.h>
#include <iostream>

const int NONE = 0;
const int IDENTITY = 1;
const int X52_SCATTER = 2; // specific to oracle x5-2
const int X52_SOCKET1_THEN_SOCKET2 = 3; // specific to oracle x5-2
const int POMELA6_SCATTER = 4;
const int TAPUZ40_SCATTER = 5;
const int TAPUZ40_CLUSTER = 6;
const int SOSCIP_CLUSTER = 100;
const int SOSCIP_CLUSTER48 = 101;
const int SOSCIP_SCATTER = 102;

// cpu sets for binding threads to cores
#ifdef THREAD_BINDING
cpu_set_t *cpusets[PHYSICAL_PROCESSORS];
#endif

void bindThread(const int tid, const int nprocessors) {
#ifdef THREAD_BINDING
    if (THREAD_BINDING != NONE) {
        if (sched_setaffinity(0, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) { // bind thread to core
            cout<<"ERROR: could not bind thread "<<tid<<" to cpuset "<<cpusets[tid%nprocessors]<<endl;
            exit(-1);
        }
        for (int i=0;i<nprocessors;++i) {
            if (CPU_ISSET_S(i, CPU_ALLOC_SIZE(nprocessors), cpusets[tid%nprocessors])) {
                //COUTATOMICTID("binding thread "<<tid<<" to cpu "<<i<<endl);
    //        } else {
    //            COUTATOMICTID("ERROR binding to cpu "<<i<<endl);
    //            exit(-1);
            }
        }
    }
#endif
}

void configureBindingPolicy(const int nprocessors) {
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
            case X52_SCATTER:
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
            case X52_SOCKET1_THEN_SOCKET2:
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
            case SOSCIP_CLUSTER:
#define SOSCIP_CLUSTER_coresPerSocket 12
#define SOSCIP_CLUSTER_threadsPerCore 8
#define SOSCIP_CLUSTER_threadsPerSocket ((SOSCIP_CLUSTER_coresPerSocket)*(SOSCIP_CLUSTER_threadsPerCore))
                j = (i%SOSCIP_CLUSTER_coresPerSocket)*SOSCIP_CLUSTER_threadsPerCore
                        + (i%SOSCIP_CLUSTER_threadsPerSocket)/SOSCIP_CLUSTER_coresPerSocket
                        + (i/SOSCIP_CLUSTER_threadsPerSocket)*SOSCIP_CLUSTER_threadsPerSocket;
            case SOSCIP_CLUSTER48:
#define SOSCIP_CLUSTER48_coresPerSocket 12
#define SOSCIP_CLUSTER48_threadsPerCore 2
#define SOSCIP_CLUSTER48_threadsPerSocket ((SOSCIP_CLUSTER48_coresPerSocket)*(SOSCIP_CLUSTER48_threadsPerCore))
                j = (i%SOSCIP_CLUSTER48_coresPerSocket)*SOSCIP_CLUSTER48_threadsPerCore
                        + (i%SOSCIP_CLUSTER48_threadsPerSocket)/SOSCIP_CLUSTER48_coresPerSocket
                        + (i/SOSCIP_CLUSTER48_threadsPerSocket)*SOSCIP_CLUSTER48_threadsPerSocket;
            case SOSCIP_SCATTER:
                // output = MOD(A1,J$5*J$4)*J$6+INT(A1/J$5/J$4) where a1=i, j4=sockets, j5=cores/socket, j6=threads/core
                j = (i%(2*SOSCIP_CLUSTER_coresPerSocket))*SOSCIP_CLUSTER_threadsPerCore + (i/SOSCIP_CLUSTER48_coresPerSocket/SOSCIP_CLUSTER_threadsPerCore);
            case NONE:
                break;
        }
        if (THREAD_BINDING != NONE) {
            CPU_SET_S(j, size, cpusets[i]);
        }
    }
#else
#define THREAD_BINDING NONE
#endif
}

#endif	/* BINDING_H */

