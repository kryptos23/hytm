/**
 * Unbalanced external binary search tree implemented using various TMs.
 *
 * Copyright (C) 2016 Trevor Brown
 */

#include "bst.h"
#include <cassert>
#include <cstdlib>
#include "../recordmgr/machineconstants.h"
#include "../globals_extern.h"
#include "../globals.h"
using namespace std;

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
    newnode->marked = false;
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

#ifdef TLE
template<class K, class V, class Compare, class RecManager>
int bst<K,V,Compare,RecManager>::rangeQuery_tle(const int tid, const K& low, const K& hi, Node<K,V> const ** result) {
    int cnt;

    block<Node<K,V> > stack (NULL);

    shmem->leaveQuiescentState(tid);
    TLEScope scope (&this->lock, MAX_FAST_HTM_RETRIES, tid, counters->pathSuccess, counters->pathFail, counters->htmAbort);

    cnt = 0;
    
    // depth first traversal (of interesting subtrees)
    stack.push(root);
    while (!stack.isEmpty()) {
        Node<K,V> * node = stack.pop();
        Node<K,V> * left = node->left;

        // if internal node, explore its children
        if (left != NULL) {
            if (node->key != this->NO_KEY && !cmp(hi, node->key)) {
                Node<K,V> *right = node->right;
                stack.push(right);
            }
            if (node->key == this->NO_KEY || cmp(low, node->key)) {
                stack.push(left);
            }

        // else if leaf node, check if we should return it
        } else {
            //visitedNodes[cnt] = node;
            if (node->key != this->NO_KEY && !cmp(node->key, low) && !cmp(hi, node->key)) {
                result[cnt] = node;
            }
        }
    }
    
    scope.end();
    shmem->enterQuiescentState(tid);
    return cnt;
}
#endif

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

#ifdef TLE
template<class K, class V, class Compare, class RecManager>
const pair<V,bool> bst<K,V,Compare,RecManager>::find_tle(const int tid, const K& key) {
    pair<V,bool> result;
    Node<K,V> *p;
    Node<K,V> *l;
    
    shmem->leaveQuiescentState(tid);
    TLEScope scope (&this->lock, MAX_FAST_HTM_RETRIES, tid, counters->pathSuccess, counters->pathFail, counters->htmAbort);

    TRACE COUTATOMICTID("find(tid="<<tid<<" key="<<key<<")"<<endl);
    // root is never retired, so we don't need to call
    // protectPointer before accessing its child pointers
    p = root->left;
    assert(p != root);
    l = p->left;
    if (l == NULL) {
        result = pair<V,bool>(NO_VALUE, false); // no keys in data structure
        
        scope.end();
        shmem->enterQuiescentState(tid);
        return result; // success
    }

    assert(shmem->isProtected(tid, l));
    while (l->left != NULL) {
        TRACE COUTATOMICTID("traversing tree; l="<<*l<<endl);
        p = l; // note: the new p is currently protected
        if (cmp(key, p->key)) {
            l = p->left;
        } else {
            l = p->right;
        }
    }
    if (key == l->key) {
        result = pair<V,bool>(l->value, true);
    } else {
        result = pair<V,bool>(NO_VALUE, false);
    }

    scope.end();
    shmem->enterQuiescentState(tid);
    return result; // success
}
#endif

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

#ifdef TLE
template<class K, class V, class Compare, class RecManager>
const V bst<K,V,Compare,RecManager>::insert_tle(const int tid, const K& key, const V& val) {
    shmem->leaveQuiescentState(tid);
    TLEScope scope (&this->lock, MAX_FAST_HTM_RETRIES, tid, counters->pathSuccess, counters->pathFail, counters->htmAbort);

    initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 0), key, val, /*1,*/ NULL, NULL);
    Node<K,V> *p = root, *l;
    l = root->left;
    if (l->left != NULL) { // the tree contains some node besides sentinels...
        p = l;
        l = l->left;    // note: l must have key infinity, and l->left must not.
        while (l->left != NULL) {
            p = l;
            if (cmp(key, p->key)) {
                l = p->left;
            } else {
                l = p->right;
            }
        }
    }
    // if we find the key in the tree already
    if (key == l->key) {
        V result = l->value;
        l->value = val;
        scope.end();
        shmem->enterQuiescentState(tid);
        return result;
    } else {
        if (l->key == NO_KEY || cmp(key, l->key)) {
            initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), l->key, l->value, /*newWeight,*/ GET_ALLOCATED_NODE_PTR(tid, 0), l);
        } else {
            initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), key, val, /*newWeight,*/ l, GET_ALLOCATED_NODE_PTR(tid, 0));
        }

        Node<K,V> *pleft = p->left;
        if (l == pleft) {
            p->left = GET_ALLOCATED_NODE_PTR(tid, 1);
        } else {
            p->right = GET_ALLOCATED_NODE_PTR(tid, 1);
        }
        scope.end();
        shmem->enterQuiescentState(tid);

        // do memory reclamation and allocation
        REPLACE_ALLOCATED_NODE(tid, 0);
        REPLACE_ALLOCATED_NODE(tid, 1);

        return NO_VALUE;
    }
}
#endif

template<class K, class V, class Compare, class RecManager>
const V bst<K,V,Compare,RecManager>::insert_stm(TM_ARGDECL_ALONE, const int tid, const K& key, const V& val) {
    shmem->leaveQuiescentState(tid);
//    TRACE COUTATOMICTID("insert_stm("<<key<<")"<<endl);
    TM_BEGIN();
    initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 0), key, val, /*1,*/ NULL, NULL);
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
        return result;
    } else {
        if (TM_SHARED_READ_L(l->key) == NO_KEY || cmp(key, TM_SHARED_READ_L(l->key))) {
            initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), TM_SHARED_READ_L(l->key), TM_SHARED_READ_L(l->value), /*newWeight,*/ GET_ALLOCATED_NODE_PTR(tid, 0), l);
        } else {
            initializeNode(tid, GET_ALLOCATED_NODE_PTR(tid, 1), key, val, /*newWeight,*/ l, GET_ALLOCATED_NODE_PTR(tid, 0));
        }

        Node<K,V> *pleft = (Node<K,V> *) TM_SHARED_READ_P(p->left);
        if (l == pleft) {
            TM_SHARED_WRITE_P(p->left, GET_ALLOCATED_NODE_PTR(tid, 1));
        } else {
            TM_SHARED_WRITE_P(p->right, GET_ALLOCATED_NODE_PTR(tid, 1));
        }
        TM_END();
        shmem->enterQuiescentState(tid);

        // do memory reclamation and allocation
        REPLACE_ALLOCATED_NODE(tid, 0);
        REPLACE_ALLOCATED_NODE(tid, 1);

        return NO_VALUE;
    }
}

#ifdef TLE
template<class K, class V, class Compare, class RecManager>
const pair<V,bool> bst<K,V,Compare,RecManager>::erase_tle(const int tid, const K& key) {
    shmem->leaveQuiescentState(tid);
    TLEScope scope (&this->lock, MAX_FAST_HTM_RETRIES, tid, counters->pathSuccess, counters->pathFail, counters->htmAbort);

    V result = NO_VALUE;
    
    Node<K,V> *gp, *p, *l;
    l = root->left;
    if (l->left == NULL) {
        scope.end();
        shmem->enterQuiescentState(tid);
        return pair<V,bool>(result, (result != NO_VALUE)); // success
    } // only sentinels in tree...
    gp = root;
    p = l;
    l = p->left;    // note: l must have key infinity, and l->left must not.
    while (l->left != NULL) {
        gp = p;
        p = l;
        if (cmp(key, p->key)) {
            l = p->left;
        } else {
            l = p->right;
        }
    }
    // if we fail to find the key in the tree
    if (key != l->key) {
        scope.end();
        shmem->enterQuiescentState(tid);
    } else {
        Node<K,V> *gpleft, *gpright;
        Node<K,V> *pleft, *pright;
        Node<K,V> *sleft, *sright;
        gpleft = gp->left;
        gpright = gp->right;
        pleft = p->left;
        pright = p->right;
        // assert p is a child of gp, l is a child of p
        Node<K,V> *s = (l == pleft ? pright : pleft);
        sleft = s->left;
        sright = s->right;
        if (p == gpleft) {
            gp->left = s;
        } else {
            gp->right = s;
        }
        result = l->value;
        scope.end();
        shmem->enterQuiescentState(tid);

        // do memory reclamation and allocation
        shmem->retire(tid, p);
        shmem->retire(tid, l);
    }
    return pair<V,bool>(result, (result != NO_VALUE));
}
#endif

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
