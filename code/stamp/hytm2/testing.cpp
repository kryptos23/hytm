/* 
 * File:   testhytm1.cpp
 * Author: trbot
 *
 * Created on March 25, 2016, 11:28 PM
 */

#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <cstdio>
#include "stm.h"

using namespace std;

volatile bool start;
long globaliters;
long globalx;
long globaly;
long globaln;


void test0(int iters) {
    // tm metadata for this thread
    const int id = 0;
    STM_THREAD_T* STM_SELF = STM_NEW_THREAD();
    STM_INIT_THREAD(STM_SELF, id);
    
    // work kernel
    long x = 0, y = 0;
    for (int i=0;i<iters;++i) {
        STM_BEGIN_WR();
        STM_WRITE_L(x, STM_READ_L(x)+1);
        STM_WRITE_L(y, STM_READ_L(y)+1);
        STM_END();
    }
    
    STM_FREE_THREAD(STM_SELF);

    cout<<"x="<<x<<" y="<<y<<"... ";
    if (x != y || x != iters) {
        cout<<"TEST FAILED (expected x=y="<<iters<<")"<<endl;
        exit(-1);
    }
    cout<<"success."<<endl;
}

void *ntest0_kernel(void* arg) {
    STM_THREAD_T* STM_SELF = (STM_THREAD_T*) arg;
    while (!start) { __sync_synchronize(); }
    
    for (int i=0;i<globaliters;++i) {
        long x=0, y=0;
        STM_BEGIN_WR();
        __sync_synchronize();
        x = STM_READ_L(globalx);
        y = STM_READ_L(globaly);
        STM_WRITE_L(globalx, x+1);
        STM_WRITE_L(globaly, y+1);
        STM_END();
#if defined(DEBUG_PRINT) || defined(DEBUG_PRINT_LOCK)
        printf("id=%ld x=%ld y=%ld\n", *((long*) arg), x, y);
#endif
    }
}

void ntest0_validate() {
    cout<<"x="<<globalx<<" y="<<globaly<<"... ";
    if (globalx != globaly || globalx != globaln*globaliters) {
        cout<<"TEST FAILED (expected x=y="<<(globaln*globaliters)<<")"<<endl;
        exit(-1);
    }
    cout<<"success."<<endl;
}

void run_test(int n, void (*validate)(void), void *(*kernel)(void*)) {
    globaln = n;
    start = 0;
    
    pthread_t* pthreads[n];
    STM_THREAD_T* stmthreads[n];
    
    for (int i=0;i<n;++i) {
        pthreads[i] = (pthread_t*) malloc(sizeof(pthread_t));
        stmthreads[i] = STM_NEW_THREAD();
        STM_INIT_THREAD(stmthreads[i], i);
        pthread_create(pthreads[i], NULL, kernel, stmthreads[i]);
    }
    
    start = 1;
    __sync_synchronize();
    
    for (int i=0;i<n;++i) {
        pthread_join(*pthreads[i], NULL);
        free(pthreads[i]);
        STM_FREE_THREAD(stmthreads[i]);
    }
    
    validate();
}

int main(int argc, char** argv) {
    const int NPROCESSORS = 8;
    STM_STARTUP();
//    int n = 2; globaliters = 1000; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
    cout<<"Main-thread tests."<<endl;
    test0(0);
    test0(1);
    test0(2);
    test0(100);
    test0(1000000);
    test0(10000000);
    cout<<"Spawned-thread tests."<<endl;
    for (int n=1;n<=NPROCESSORS;++n) {
        cout<<n<<" threads"<<endl;
        globaliters = 0; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 1; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 10; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 100; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 1000; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 10000; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        globaliters = 100000; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel);
        if (n <= 8) { globaliters = 1000000/n; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel); }
        if (n <= 2) { globaliters = 10000000/n; globalx = globaly = 0; run_test(n, ntest0_validate, ntest0_kernel); }
    }
    STM_SHUTDOWN();
    return 0;
}

