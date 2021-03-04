/**
 * Unbalanced external binary search tree implemented using various TMs.
 *
 * Copyright (C) 2016 Trevor Brown
 */

#pragma once

#include <string>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <setjmp.h>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdexcept>
#include <bitset>
#include <cassert>
#include <cstdlib>
#include <atomic>
#include "recordmgr/machineconstants.h"
#include "recordmgr/record_manager.h"
#include "debugcounters.h"
#include "random.h"
#include "dsbench_tm.h"
#include "recordmgr/machineconstants.h"
#include "globals_extern.h"
#include "globals.h"

using namespace std;

#define MAX_NODES 6

template <class K, class V>
class Node {
public:
    V value;
    K key;
    Node<K,V> * volatile left;
    Node<K,V> * volatile right;

    Node() {
        // left blank for efficiency with custom allocator
    }
    Node(const Node& node) {
        // left blank for efficiency with custom allocator
    }

    K getKey() { return key; }
    V getValue() { return value; }

    friend ostream& operator<<(ostream& os, const Node<K,V>& obj) {
        ios::fmtflags f( os.flags() );
        os<<"[key="<<obj.key;
        os<<" left@0x"<<hex<<(long)(obj.left);
        os<<" right@0x"<<hex<<(long)(obj.right);
        os<<"]"<<"@0x"<<hex<<(long)(&obj);
        os.flags(f);
        return os;
    }

    // somewhat slow version that detects cycles in the tree
    void printTreeFile(ostream& os, set< Node<K,V>* > *seen) {
        os<<"(";
        os<<key;
        os<<",";
        if (left == NULL) {
            os<<"-";
        } else if (seen->find((Node<K,V> *)left) != seen->end()) {   // for finding cycles
            os<<"!"; // cycle!                          // for finding cycles
        } else {
            seen->insert((Node<K,V> *) left);
            left->printTreeFile(os, seen);
        }
        os<<",";
        if (right == NULL) {
            os<<"-";
        } else if (seen->find((Node<K,V> *)right) != seen->end()) {  // for finding cycles
            os<<"!"; // cycle!                          // for finding cycles
        } else {
            seen->insert((Node<K,V> *)right);
            right->printTreeFile(os, seen);
        }
        os<<")";
    }

    void printTreeFile(ostream& os) {
        set< Node<K,V>* > seen;
        printTreeFile(os, &seen);
    }

};

template <class K, class V, class Compare, class RecManager>
class bst {
private:
    PAD;
    RecManager * const shmem;
    PAD;

    Node<K,V> *root;        // actually const
    Compare cmp;
    PAD;

    // similarly, allocatedNodes[tid*PREFETCH_SIZE_WORDS+i] = an allocated node for i = 0..MAX_NODES-2
    // Node<K,V> **allocatedNodes;
    // PAD;
    // #define GET_ALLOCATED_NODE_PTR(tid, i) allocatedNodes[tid*(PREFETCH_SIZE_WORDS+MAX_NODES)+i]
    // #define REPLACE_ALLOCATED_NODE(tid, i) { GET_ALLOCATED_NODE_PTR(tid, i) = allocateNode(tid); /*GET_ALLOCATED_NODE_PTR(tid, i)->left.store((uintptr_t) NULL, memory_order_relaxed);*/ }

    // debug info
    debugCounters<PHYSICAL_PROCESSORS> counters __attribute__((aligned(BYTES_IN_CACHE_LINE)));
    PAD;

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
    PAD;

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
        // allocatedNodes = new Node<K,V> * [MAX_TID_POW2*(PREFETCH_SIZE_WORDS+MAX_NODES) + 2*PREFETCH_SIZE_WORDS /* for padding via shifting */] + PREFETCH_SIZE_WORDS /* shift to pad */;
        // for (int tid=0;tid<MAX_TID_POW2;++tid) {
        //     GET_ALLOCATED_NODE_PTR(tid, 0) = NULL; // set up initial conditions for initThread(tid)
        // }
    }
    /**
     * This function must be called once by each thread that will
     * invoke any functions on this class.
     *
     * It must be okay that we do this with the main thread and later with another thread!!!
     */
    void initThread(const int tid) {
        shmem->initThread(tid);
        // if (GET_ALLOCATED_NODE_PTR(tid, 0) == NULL) {
        //     for (int i=0;i<MAX_NODES;++i) {
        //         REPLACE_ALLOCATED_NODE(tid, i);
        //     }
        // }
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
        cout<<"destructor bst"<<endl;
        // free every node currently in the data structure.
        // an easy DFS, freeing from the leaves up, handles all nodes.
        int numNodes = 0;
        dfsDeallocateBottomUp(root, &numNodes);
        VERBOSE DS_DEBUG COUTATOMIC(" deallocated nodes "<<numNodes<<endl);
        // for (int tid=0;tid<shmem->NUM_PROCESSES;++tid) {
        //     for (int i=0;i<MAX_NODES;++i) {
        //         shmem->deallocate(tid, GET_ALLOCATED_NODE_PTR(tid, i));
        //     }
        // }
        delete shmem;
        // delete[] (allocatedNodes - PREFETCH_SIZE_WORDS); // unshift before free (since array is shifted upon alloc)
    }

    Node<K,V> *getRoot(void) { return root; }
    const V insert_stm(TM_ARGDECL_ALONE, const int tid, const K& key, const V& val);
    const pair<V,bool> erase_stm(TM_ARGDECL_ALONE, const int tid, const K& key);
    const pair<V,bool> find_stm(TM_ARGDECL_ALONE, const int tid, const K& key);
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

template<class K, class V, class Compare, class RecManager>
inline Node<K,V>* bst<K,V,Compare,RecManager>::allocateNode(
            const int tid) {
    Node<K,V> *newnode = shmem->template allocate<Node<K,V> >(tid);
    if (newnode == NULL) {
        COUTATOMICTID("ERROR: could not allocate node"<<endl);
        exit(-1);
    }
    return newnode;
}

template<class K, class V, class Compare, class RecManager>
inline Node<K,V>* bst<K,V,Compare,RecManager>::initializeNode(
            const int tid,
            Node<K,V> * const newnode,
            const K& key,
            const V& value,
            Node<K,V> * const left,
            Node<K,V> * const right) {
    newnode->key = key;
    newnode->value = value;
    // note: synchronization is not necessary for the following accesses,
    // since a memory barrier will occur before this object becomes reachable
    // from an entry point to the data structure.
    newnode->left = left;
    newnode->right = right;
    return newnode;
}

template<class K, class V, class Compare, class RecManager>
long long bst<K,V,Compare,RecManager>::debugKeySum(Node<K,V> * node) {
    if (node == NULL) return 0;
    if (node->left == NULL) return (long long) node->key;
    return debugKeySum(node->left)
         + debugKeySum(node->right);
}

template<class K, class V, class Compare, class RecManager>
bool bst<K,V,Compare,RecManager>::validate(Node<K,V> * const node, const int currdepth, const int leafdepth) {
    return true;
}

template<class K, class V, class Compare, class RecManager>
bool bst<K,V,Compare,RecManager>::validate(const long long keysum, const bool checkkeysum) {
    return true;
}

template<class K, class V, class Compare, class RecManager>
inline int bst<K,V,Compare,RecManager>::size() {
    return computeSize(root->left->left);
}

template<class K, class V, class Compare, class RecManager>
inline int bst<K,V,Compare,RecManager>::computeSize(Node<K,V> * const root) {
    if (root == NULL) return 0;
    if (root->left != NULL) { // if internal node
        return computeSize(root->left)
                + computeSize(root->right);
    } else { // if leaf
        return 1;
//        printf(" %d", root->key);
    }
}

//#define PTR_STACK_SIZE 1024
//
//template <typename T>
//class ptrstack { // stack implemented as an array
//private:
//    T * data[PTR_STACK_SIZE];
//    int size;
//public:
//    ptrstack() { size = 0; }
//    ~ptrstack() { assert(size == 0); }
//    bool isEmpty() { return size == 0; }
//    // precondition: size < PTR_STACK_SIZE
//    void push(T * const obj) {
//        assert(size < PTR_STACK_SIZE);
//        const int sz = size;
//        data[size] = obj;
//        size = sz+1;
//    }
//    // precondition: size > 0
//    T* pop() {
//        assert(size > 0);
//        const int sz = size-1;
//        size = sz;
//        return data[sz];
//    }
//    void clear() {
//        size = 0;
//    }
//};

template<class K, class V, class Compare, class RecManager>
int bst<K,V,Compare,RecManager>::rangeUpdate_stm(TM_ARGDECL_ALONE, const int tid, const K& low, const K& hi) {
    int result = 0;
//    cout<<"rangeUpdate_stm(lo="<<low<<", hi="<<hi<<")"<<endl;
//    ptrstack<Node<K,V> > stack;
    block<Node<K,V> > stack (NULL);

    shmem->leaveQuiescentState(tid);
    TM_BEGIN();

    // depth first traversal (of interesting subtrees)
    //stack.clear();
    stack.clearWithoutFreeingElements();
    stack.push((Node<K,V> *) TM_SHARED_READ_P(root));
    while (!stack.isEmpty()) {
        Node<K,V> * node = stack.pop();
        Node<K,V> * left = (Node<K,V> *) TM_SHARED_READ_P(node->left);

        // if internal node, explore its children
        K key = (K) TM_SHARED_READ_L(node->key);
        if (left != NULL) {
            if (key != this->NO_KEY && !cmp(hi, key)) {
                Node<K,V> *right = (Node<K,V> *) TM_SHARED_READ_P(node->right);
                stack.push(right);
            }
            if (key == this->NO_KEY || cmp(low, key)) {
                stack.push(left);
            }

        // else if leaf node, check if we should return it
        } else {
            if (key != this->NO_KEY && !cmp(key, low) && !cmp(hi, key)) {
                V v = (V) TM_SHARED_READ_L(node->value);
                TM_SHARED_WRITE_L(node->value, v+1);
                ++result;
            }
        }
    }
    TM_END();
    shmem->enterQuiescentState(tid);

    // success
    return result;
}

template<class K, class V, class Compare, class RecManager>
const pair<V,bool> bst<K,V,Compare,RecManager>::find_stm(TM_ARGDECL_ALONE, const int tid, const K& key) {
    pair<V,bool> result;
    Node<K,V> *p;
    Node<K,V> *l;

    shmem->leaveQuiescentState(tid);
    TM_BEGIN_RO();

    TRACE COUTATOMICTID("find(tid="<<tid<<" key="<<key<<")"<<endl);
    // root is never retired, so we don't need to call
    // protectPointer before accessing its child pointers
    p = (Node<K,V> *) TM_SHARED_READ_P(root);
    p = (Node<K,V> *) TM_SHARED_READ_P(p->left);
    l = (Node<K,V> *) TM_SHARED_READ_P(p->left);
    if (l == NULL) {
        result = pair<V,bool>(NO_VALUE, false); // no keys in data structure
        TM_END();
        shmem->enterQuiescentState(tid);
        return result; // success
    }

    while (TM_SHARED_READ_P(l->left) != NULL) {
        TRACE COUTATOMICTID("traversing tree; l="<<*l<<endl);
        p = l; // note: the new p is currently protected
        if (cmp(key, TM_SHARED_READ_L(p->key))) {
            l = (Node<K,V> *) TM_SHARED_READ_P(p->left);
        } else {
            l = (Node<K,V> *) TM_SHARED_READ_P(p->right);
        }
    }
    if (key == TM_SHARED_READ_L(l->key)) {
        result = pair<V,bool>(TM_SHARED_READ_L(l->value), true);
    } else {
        result = pair<V,bool>(NO_VALUE, false);
    }

    TM_END();
    shmem->enterQuiescentState(tid);
    return result; // success
}

template<class K, class V, class Compare, class RecManager>
const V bst<K,V,Compare,RecManager>::insert_stm(TM_ARGDECL_ALONE, const int tid, const K& key, const V& val) {
    shmem->leaveQuiescentState(tid);
//    TRACE COUTATOMICTID("insert_stm("<<key<<")"<<endl);

    Node<K,V> * node0 = allocateNode(tid);
    Node<K,V> * node1 = allocateNode(tid);

    SOFTWARE_BARRIER;

    initializeNode(tid, node0, key, val, NULL, NULL);
    // initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 0), key, val, /*1,*/ NULL, NULL);

    TM_BEGIN();

    Node<K,V> *p = (Node<K,V> *) TM_SHARED_READ_P(root);
    Node<K,V> *l = (Node<K,V> *) TM_SHARED_READ_P(p->left);
    if (TM_SHARED_READ_P(l->left) != NULL) { // the tree contains some node besides sentinels...
        p = l;
        l = (Node<K,V> *) TM_SHARED_READ_P(l->left);    // note: l must have key infinity, and l->left must not.
        while (TM_SHARED_READ_P(l->left) != NULL) {
            p = l;
            if (cmp(key, TM_SHARED_READ_L(p->key))) {
                l = (Node<K,V> *) TM_SHARED_READ_P(p->left);
            } else {
                l = (Node<K,V> *) TM_SHARED_READ_P(p->right);
            }
        }
    }
    // if we find the key in the tree already
    if (key == TM_SHARED_READ_L(l->key)) {
        V result = TM_SHARED_READ_L(l->value);
        TM_SHARED_WRITE_L(l->value, val);
        TM_END();

        shmem->enterQuiescentState(tid);
        shmem->deallocate(tid, node0);
        shmem->deallocate(tid, node1);
        return result;
    } else {
        if (TM_SHARED_READ_L(l->key) == NO_KEY || cmp(key, TM_SHARED_READ_L(l->key))) {
            initializeNode(tid, node1, TM_SHARED_READ_L(l->key), TM_SHARED_READ_L(l->value), node0, l);
            // initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), TM_SHARED_READ_L(l->key), TM_SHARED_READ_L(l->value), /*newWeight,*/ GET_ALLOCATED_NODE_PTR(tid, 0), l);
        } else {
            initializeNode(tid, node1, key, val, l, node0);
            // initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), key, val, /*newWeight,*/ l, GET_ALLOCATED_NODE_PTR(tid, 0));
        }

        Node<K,V> *pleft = (Node<K,V> *) TM_SHARED_READ_P(p->left);
        if (l == pleft) {
            TM_SHARED_WRITE_P(p->left, node1);
            // TM_SHARED_WRITE_P(p->left, GET_ALLOCATED_NODE_PTR(tid, 1));
        } else {
            TM_SHARED_WRITE_P(p->right, node1);
            // TM_SHARED_WRITE_P(p->right, GET_ALLOCATED_NODE_PTR(tid, 1));
        }

        TM_END();
        shmem->enterQuiescentState(tid);

        SOFTWARE_BARRIER;

        // do memory reclamation and allocation
        // REPLACE_ALLOCATED_NODE(tid, 0);
        // REPLACE_ALLOCATED_NODE(tid, 1);

        return NO_VALUE;
    }
}

template<class K, class V, class Compare, class RecManager>
const pair<V,bool> bst<K,V,Compare,RecManager>::erase_stm(TM_ARGDECL_ALONE, const int tid, const K& key) {
    shmem->leaveQuiescentState(tid);
//    TRACE COUTATOMICTID("erase_stm("<<key<<")"<<endl);
    TM_BEGIN();

    V result = NO_VALUE;

    Node<K,V> *gp, *p, *l;
    l = (Node<K,V> *) TM_SHARED_READ_P(((Node<K,V> *) TM_SHARED_READ_P(root))->left);
    if (TM_SHARED_READ_P(l->left) == NULL) {
        TM_END();
        shmem->enterQuiescentState(tid);
        return pair<V,bool>(result, (result != NO_VALUE)); // success
    } // only sentinels in tree...
    gp = (Node<K,V> *) TM_SHARED_READ_P(root);
    p = l;
    l = (Node<K,V> *) TM_SHARED_READ_P(p->left); // note: l must have key infinity, and l->left must not.
    while (TM_SHARED_READ_P(l->left) != NULL) {
        gp = p;
        p = l;
        if (cmp(key, TM_SHARED_READ_L(p->key))) {
            l = (Node<K,V> *) TM_SHARED_READ_P(p->left);
        } else {
            l = (Node<K,V> *) TM_SHARED_READ_P(p->right);
        }
    }
    // if we fail to find the key in the tree
    if (key != TM_SHARED_READ_L(l->key)) {
        TM_END();
        shmem->enterQuiescentState(tid);
    } else {
        Node<K,V> *gpleft, *gpright;
        Node<K,V> *pleft, *pright;
        Node<K,V> *sleft, *sright;
        gpleft = (Node<K,V> *) TM_SHARED_READ_P(gp->left);
        gpright = (Node<K,V> *) TM_SHARED_READ_P(gp->right);
        pleft = (Node<K,V> *) TM_SHARED_READ_P(p->left);
        pright = (Node<K,V> *) TM_SHARED_READ_P(p->right);
        // assert p is a child of gp, l is a child of p
        Node<K,V> *s = (l == pleft ? pright : pleft);
        sleft = (Node<K,V> *) TM_SHARED_READ_P(s->left);
        sright = (Node<K,V> *) TM_SHARED_READ_P(s->right);
        if (p == gpleft) {
            TM_SHARED_WRITE_P(gp->left, s);
        } else {
            TM_SHARED_WRITE_P(gp->right, s);
        }
        result = TM_SHARED_READ_L(l->value);
        TM_END();
        shmem->enterQuiescentState(tid);

        // do memory reclamation and allocation
        shmem->retire(tid, p);
        shmem->retire(tid, l);
    }
    return pair<V,bool>(result, (result != NO_VALUE));
}
