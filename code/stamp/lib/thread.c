/* =============================================================================
 *
 * thread.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 *
 * ------------------------------------------------------------------------
 *
 * For the license of ssca2, please see ssca2/COPYRIGHT
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 *
 * ------------------------------------------------------------------------
 *
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 *
 * ------------------------------------------------------------------------
 *
 * Unless otherwise noted, the following license applies to STAMP files:
 *
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <assert.h>
#include <stdlib.h>
#include <sched.h>
#include "thread.h"
#include "types.h"
//#include "rapl.h"
#include "print_elapsed_extern.h"

static THREAD_LOCAL_T    global_threadId;
static long              global_numThread       = 1;
static THREAD_BARRIER_T* global_barrierPtr      = NULL;
static long*             global_threadIds       = NULL;
static THREAD_ATTR_T     global_threadAttr;
static THREAD_T*         global_threads         = NULL;
static void            (*global_funcPtr)(void*) = NULL;
static void*             global_argPtr          = NULL;
static volatile bool_t   global_doShutdown      = FALSE;

static void
threadWait (void* argPtr)
{
    long threadId = *(long*)argPtr;

    if (threadId == 0) PRINT_ELAPSED_FROM_START();

    THREAD_LOCAL_SET(global_threadId, (long)threadId);
    if (threadId == 0) PRINT_ELAPSED_FROM_START();
//    cpu_set_t my_set;
//    CPU_ZERO(&my_set);
//    CPU_SET(threadId % 8, &my_set);
//    sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

    while (1) {
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for start parallel */
        if (global_doShutdown) {
            break;
        }
        if (threadId == 0) PRINT_ELAPSED_FROM_START();
        // { printf("BAR_ENTERED tid=%ld %s:%d %dms\n", threadId, __FILE__, __LINE__, (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ____executionStartTimeDebug).count()); }
        global_funcPtr(global_argPtr);
        // { printf("DONE tid=%ld %s:%d %dms\n", threadId, __FILE__, __LINE__, (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ____executionStartTimeDebug).count()); }
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for end parallel */
        // { printf("BAR_LEFT tid=%ld %s:%d %dms\n", threadId, __FILE__, __LINE__, (int) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - ____executionStartTimeDebug).count()); }
        if (threadId == 0) {
//        	endEnergy();
            break;
        }
    }
    //if (threadId == 0) PRINT_ELAPSED_FROM_START();
}

void thread_startup (long numThread)
{
    long i;

    global_numThread = numThread;
    global_doShutdown = FALSE;

    /* Set up barrier */
    PRINT_ELAPSED_FROM_START();
    assert(global_barrierPtr == NULL);
    global_barrierPtr = THREAD_BARRIER_ALLOC(numThread);
    assert(global_barrierPtr);
    PRINT_ELAPSED_FROM_START();
    THREAD_BARRIER_INIT(global_barrierPtr, numThread);
    PRINT_ELAPSED_FROM_START();

    /* Set up ids */
    THREAD_LOCAL_INIT(global_threadId);
    PRINT_ELAPSED_FROM_START();
    assert(global_threadIds == NULL);
    global_threadIds = (long*)malloc(numThread * sizeof(long));
    assert(global_threadIds);
    PRINT_ELAPSED_FROM_START();
    for (i = 0; i < numThread; i++) {
        global_threadIds[i] = i;
    }
    PRINT_ELAPSED_FROM_START();

    /* Set up thread list */
    assert(global_threads == NULL);
    global_threads = (THREAD_T*)malloc(numThread * sizeof(THREAD_T));
    assert(global_threads);
    PRINT_ELAPSED_FROM_START();

//	startEnergy();

    /* Set up pool */
    THREAD_ATTR_INIT(global_threadAttr);
    for (i = 1; i < numThread; i++) {
        THREAD_CREATE(global_threads[i],
                      global_threadAttr,
                      &threadWait,
                      &global_threadIds[i]);
    }
    PRINT_ELAPSED_FROM_START();

    /*
     * Wait for primary thread to call thread_start
     */
}

void thread_start (void (*funcPtr)(void*), void* argPtr)
{
    global_funcPtr = funcPtr;
    global_argPtr = argPtr;

    long threadId = 0; /* primary */
    threadWait((void*)&threadId);
}

void thread_shutdown ()
{
    /* Make secondary threads exit wait() */
    global_doShutdown = TRUE;
    THREAD_BARRIER(global_barrierPtr, 0);

    long numThread = global_numThread;

    long i;
    for (i = 1; i < numThread; i++) {
        THREAD_JOIN(global_threads[i]);
    }

    THREAD_BARRIER_FREE(global_barrierPtr);
    global_barrierPtr = NULL;

    free(global_threadIds);
    global_threadIds = NULL;

    free(global_threads);
    global_threads = NULL;

    global_numThread = 1;
}

barrier_t *barrier_alloc() {
    return (barrier_t *)malloc(sizeof(barrier_t));
}

void barrier_free(barrier_t *b) {
    free(b);
}

void barrier_init(barrier_t *b, int n) {
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    /* One more thread through */
    b->crossing++;
    /* If not all here, wait */
    if (b->crossing < b->count) {
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        /* Reset for next time */
        b->crossing = 0;
        pthread_cond_broadcast(&b->complete);
    }
    pthread_mutex_unlock(&b->mutex);
}

void thread_barrier_wait()
{
    long threadId = thread_getId();
    THREAD_BARRIER(global_barrierPtr, threadId);
}

long thread_getId()
{
    return (long)THREAD_LOCAL_GET(global_threadId);
}

long thread_getNumThread()
{
    return global_numThread;
}