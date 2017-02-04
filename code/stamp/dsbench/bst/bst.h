/**
 * Unbalanced external binary search tree implemented using various TMs.
 *
 * Copyright (C) 2016 Trevor Brown
 */

#ifndef BST_H
#define	BST_H

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <setjmp.h>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdexcept>
#include <bitset>
#include "../recordmgr/machineconstants.h"
#include "../recordmgr/record_manager.h"
#include "../debugcounters.h"
#include "../common/random.h"
#ifdef TLE
#include "../tle.h"
#endif
#include "node.h"
#include "../dsbench_tm.h"

using namespace std;

template <class K, class V, class Compare, class RecManager>
class bst {
private:
    RecManager * const shmem;
    volatile int lock; // used for TLE

    Node<K,V> *root;        // actually const
    Compare cmp;
    
    // similarly, allocatedNodes[tid*PREFETCH_SIZE_WORDS+i] = an allocated node for i = 0..MAX_NODES-2
    Node<K,V> **allocatedNodes;
    #define GET_ALLOCATED_NODE_PTR(tid, i) allocatedNodes[tid*(PREFETCH_SIZE_WORDS+MAX_NODES)+i]
    #define REPLACE_ALLOCATED_NODE(tid, i) { GET_ALLOCATED_NODE_PTR(tid, i) = allocateNode(tid); /*GET_ALLOCATED_NODE_PTR(tid, i)->left.store((uintptr_t) NULL, memory_order_relaxed);*/ }
    
    // debug info
    debugCounters<PHYSICAL_PROCESSORS> counters __attribute__((aligned(PREFETCH_SIZE_BYTES)));
    
    #define IS_SENTINEL(node, parent) ((node)->key == NO_KEY || (parent)->key == NO_KEY)
    inline Node<K,V>* allocateNode(const int tid);
    inline Node<K,V>* initializeNode(const int, Node<K,V> * const, const K&, const V&, Node<K,V> * const, Node<K,V> * const);
    inline int computeSize(Node<K,V>* node);
    
    long long debugKeySum(Node<K,V> * node);
    bool validate(Node<K,V> * const node, const int currdepth, const int leafdepth);

public:
    const K& NO_KEY;
    const V& NO_VALUE;
    const V& RETRY;
    bst(const K& _NO_KEY,
                const V& _NO_VALUE,
                const V& _RETRY,
                const int numProcesses)
            : NO_KEY(_NO_KEY)
            , NO_VALUE(_NO_VALUE)
            , RETRY(_RETRY)
            , shmem(new RecManager(numProcesses, SIGQUIT)) {
        VERBOSE DS_DEBUG COUTATOMIC("constructor bst"<<endl);
        const int tid = 0;
        shmem->enterQuiescentState(tid); // enter an initial quiescent state.
        Node<K,V> *rootleft = initializeNode(tid, allocateNode(tid), NO_KEY, NO_VALUE, NULL, NULL);
        root = initializeNode(tid, allocateNode(tid), NO_KEY, NO_VALUE, rootleft, NULL);
        cmp = Compare();
        allocatedNodes = new Node<K,V>*[numProcesses*(PREFETCH_SIZE_WORDS+MAX_NODES)];
        for (int tid=0;tid<numProcesses;++tid) {
            GET_ALLOCATED_NODE_PTR(tid, 0) = NULL; // set up initial conditions for initThread(tid)
        }
    }
    /**
     * This function must be called once by each thread that will
     * invoke any functions on this class.
     * 
     * It must be okay that we do this with the main thread and later with another thread!!!
     */
    void initThread(const int tid) {
        shmem->initThread(tid);
        if (GET_ALLOCATED_NODE_PTR(tid, 0) == NULL) {
            for (int i=0;i<MAX_NODES;++i) {
                REPLACE_ALLOCATED_NODE(tid, i);
            }
        }
    }
    
    void dfsDeallocateBottomUp(Node<K,V> * const u, int *numNodes) {
        if (u == NULL) return;
        if (u->left != NULL) {
            dfsDeallocateBottomUp(u->left, numNodes);
            dfsDeallocateBottomUp(u->right, numNodes);
        }
        DS_DEBUG ++(*numNodes);
        shmem->deallocate(0 /* tid */, u);
    }
    ~bst() {
        VERBOSE DS_DEBUG COUTATOMIC("destructor bst");
        // free every node currently in the data structure.
        // an easy DFS, freeing from the leaves up, handles all nodes.
        int numNodes = 0;
        dfsDeallocateBottomUp(root, &numNodes);
        VERBOSE DS_DEBUG COUTATOMIC(" deallocated nodes "<<numNodes<<endl);
        for (int tid=0;tid<shmem->NUM_PROCESSES;++tid) {
            for (int i=0;i<MAX_NODES;++i) {
                shmem->deallocate(tid, GET_ALLOCATED_NODE_PTR(tid, i));
            }
        }
        delete shmem;
    }

    Node<K,V> *getRoot(void) { return root; }
    const V insert_tle(const int tid, const K& key, const V& val);
    const V insert_stm(TM_ARGDECL_ALONE, const int tid, const K& key, const V& val);
    const pair<V,bool> erase_tle(const int tid, const K& key);
    const pair<V,bool> erase_stm(TM_ARGDECL_ALONE, const int tid, const K& key);
    const pair<V,bool> find_tle(const int tid, const K& key);
    const pair<V,bool> find_stm(TM_ARGDECL_ALONE, const int tid, const K& key);
    int rangeQuery_tle(const int tid, const K& low, const K& hi, Node<K,V> const ** result);
    int rangeQuery_stm(TM_ARGDECL_ALONE, const int tid, const K& low, const K& hi, Node<K,V> const ** result);
    int rangeUpdate_stm(TM_ARGDECL_ALONE, const int tid, const K& low, const K& hi);
    int size(void); /** warning: size is a LINEAR time operation, and does not return consistent results with concurrency **/
    
    void debugPrintAllocatorStatus() {
        shmem->printStatus();
    }
    void debugPrintToFile(string prefix, long id1, string infix, long id2, string suffix) {
        stringstream ss;
        ss<<prefix<<id1<<infix<<id2<<suffix;
        COUTATOMIC("print to filename \""<<ss.str()<<"\""<<endl);
        fstream fs (ss.str().c_str(), fstream::out);
        root->printTreeFile(fs);
        fs.close();
    }
    void debugPrintToFileWeight(string prefix, long id1, string infix, long id2, string suffix) {
        stringstream ss;
        ss<<prefix<<id1<<infix<<id2<<suffix;
        COUTATOMIC("print to filename \""<<ss.str()<<"\""<<endl);
        fstream fs (ss.str().c_str(), fstream::out);
        root->printTreeFileWeight(fs);
        fs.close();
    }
    void clearCounters() {
        counters.clear();
        shmem->clearCounters();
        STM_CLEAR_COUNTERS();
    }
    debugCounters<PHYSICAL_PROCESSORS>& debugGetCounters() {
        return counters;
    }
    RecManager * const debugGetShmem() {
        return shmem;
    }
    
    bool validate(const long long keysum, const bool checkkeysum);
    long long debugKeySum() {
        return debugKeySum(root->left->left);
        //return debugKeySum(root->left);
    }
};

#endif	/* BST_H */

