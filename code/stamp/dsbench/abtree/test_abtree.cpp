/* 
 * File:   test_abtree.cpp
 * Author: trbot
 *
 * Created on October 23, 2015, 1:59 PM
 */

#include <cstdlib>
#include <iostream>
#include <atomic>
#include "../debugcounters.h"
#include "../globals.h"
#include "../globals_extern.h"
#include "../recordmgr/record_manager.h"
#include "abtree_impl.h"

using namespace std;

#define PASTE2(a, b) a##b
#define PASTE(a, b) PASTE2(a, b)
#define STRINGIFY2(a) #a
#define STRINGIFY(a) STRINGIFY2(a)
#define exp(val, expval) { \
    void * ___val = (void *) (val); \
    void * ___exp = (void *) (expval); \
    if (___val == ___exp) { \
        cout<<"Passed: "; \
    } else { \
        cout<<"FAILED: "; \
    } \
    cout<<STRINGIFY(val)<<" == "<<(long long) ___val<<" compared with " \
        <<STRINGIFY(expval)<<" == "<<(long long) ___exp<<endl; \
    cout<<"tree: "; \
    tree->debugPrint(); \
    cout<<endl; \
    if (___val != ___exp) { \
        exit(-1); \
    } \
}

#define DEFAULT_SUSPECTED_SIGNAL SIGQUIT
#define ABTREE_NODE_MIN_DEGREE 4
#define ABTREE_NODE_DEGREE 8
#define DS_DECLARATION abtree<ABTREE_NODE_DEGREE, test_type, less<test_type>, MemMgmt>

#define INSERT(x, y, retval) { \
    exp(tree->insert(tid, (x), (void*) (y)), (retval)); \
    if ((void *) (retval) == tree->NO_VALUE) { \
        keysum += (x); \
    } \
    if (!tree->validate(keysum, true)) { \
        return -1; \
    } \
}

#define ERASE(x, retval) { \
    exp(tree->erase(tid, (x)).first, (retval)); \
    if ((void *) (retval) != tree->NO_VALUE) { \
        keysum -= (x); \
    } \
    if (!tree->validate(keysum, true)) { \
        return -1; \
    } \
}

/*
 * 
 */
int main(int argc, char** argv) {
    typedef long long test_type;
    typedef reclaimer_none<test_type> Reclaim;
    typedef allocator_new<test_type> Alloc;
    typedef pool_none<test_type> Pool;
    typedef record_manager<Reclaim, Alloc, Pool, abtree_Node<ABTREE_NODE_DEGREE, test_type>, abtree_SCXRecord<ABTREE_NODE_DEGREE, test_type> > MemMgmt;
    const test_type NO_KEY = -1;
    WORK_THREADS = 1;
    RQ_THREADS = 0;
    MAX_FAST_HTM_RETRIES = -1;
    MAX_SLOW_HTM_RETRIES = -1;
    DS_DECLARATION * tree = new DS_DECLARATION(WORK_THREADS, DEFAULT_SUSPECTED_SIGNAL, NO_KEY, ABTREE_NODE_MIN_DEGREE);
    
    const int tid = 0;
    long long keysum = 0;
    
    TRACE_TOGGLE;
    
    tree->initThread(tid);
    exp(tree->find(tid, 1).second, false);
    exp(tree->find(tid, 2).second, false);
    exp(tree->find(tid, 3).second, false);
    exp(tree->find(tid, 4).second, false);
    exp(tree->find(tid, 5).second, false);
    exp(tree->find(tid, 6).second, false);
    exp(tree->find(tid, 7).second, false);
    exp(tree->find(tid, 8).second, false);
    exp(tree->find(tid, 9).second, false);
    cout<<"FINISHED find false test."<<endl<<endl;
    
    INSERT(1, 1, tree->NO_VALUE);
    exp(tree->find(tid, 1).first, 1);
    INSERT(1, 2, 1);
    INSERT(1, 3, 2);
    exp(tree->find(tid, 1).first, 3);
    cout<<"FINISHED simple insert test."<<endl<<endl;

    ERASE(1, 3);
    cout<<"FINISHED erase test."<<endl<<endl;
    
    INSERT(1, 1, tree->NO_VALUE);
    exp(tree->find(tid, 1).first, 1);
    INSERT(2, 2, tree->NO_VALUE);
    exp(tree->find(tid, 2).first, 2);
    INSERT(3, 3, tree->NO_VALUE);
    exp(tree->find(tid, 3).first, 3);
    INSERT(4, 4, tree->NO_VALUE);
    exp(tree->find(tid, 4).first, 4);
    INSERT(5, 5, tree->NO_VALUE);
    exp(tree->find(tid, 5).first, 5);
    INSERT(6, 6, tree->NO_VALUE);
    exp(tree->find(tid, 6).first, 6);
    INSERT(7, 7, tree->NO_VALUE);
    exp(tree->find(tid, 7).first, 7);
    INSERT(8, 8, tree->NO_VALUE);
    exp(tree->find(tid, 8).first, 8);
    INSERT(9, 9, tree->NO_VALUE);
    exp(tree->find(tid, 9).first, 9);
    
    exp(tree->find(tid, 1).first, 1);
    exp(tree->find(tid, 2).first, 2);
    exp(tree->find(tid, 3).first, 3);
    exp(tree->find(tid, 4).first, 4);
    exp(tree->find(tid, 5).first, 5);
    exp(tree->find(tid, 6).first, 6);
    exp(tree->find(tid, 7).first, 7);
    exp(tree->find(tid, 8).first, 8);
    exp(tree->find(tid, 9).first, 9);
    cout<<"FINISHED overflow insert test."<<endl<<endl;
    
    // test redistributeSibling
    ERASE(4, 4);
    cout<<"FINISHED redistributeSibling test."<<endl<<endl;

    // test tagJoinParent
    INSERT(10, 10, tree->NO_VALUE);
    INSERT(11, 11, tree->NO_VALUE);
    INSERT(12, 12, tree->NO_VALUE);
    INSERT(13, 13, tree->NO_VALUE);
    INSERT(14, 14, tree->NO_VALUE);
    cout<<"FINISHED tagJoinParent test."<<endl<<endl;

    // test tagSplit
    for (long long i=15;i<15+6*4;++i) {
        INSERT(i, i, tree->NO_VALUE);
    }
    cout<<"FINISHED tagSplit test."<<endl<<endl;
    
    //tree->debugPrint();
    // (1,[18],(3,[5,10,14],(4,[1,2,3,5],1,2,3,5),(4,[6,7,8,9],6,7,8,9),(4,[10,11,12,13],10,11,12,13),(4,[14,15,16,17],14,15,16,17)),(4,[22,26,30,34],(4,[18,19,20,21],18,19,20,21),(4,[22,23,24,25],22,23,24,25),(4,[26,27,28,29],26,27,28,29),(4,[30,31,32,33],30,31,32,33),(5,[34,35,36,37,38],34,35,36,37,38)))
    
    // test joinSibling leaf
    ERASE(1, 1);
    cout<<"FINISHED joinSibling test."<<endl<<endl;
    
    // test joinSibling internal, rootJoinParent
    for (long long i=15+6*4-1;i>15+5*4;--i) {
        ERASE(i, i);
    }
    cout<<"FINISHED joinSibling internal, rootJoinParent test."<<endl<<endl;
    
    // test redistributeSibling internal
    for (long long i=15+5*4+1;i<15+8*4;++i) {
        INSERT(i, i, tree->NO_VALUE);
    }
    ERASE(2, 2);
    ERASE(3, 3);
    ERASE(5, 5);
    ERASE(6, 6);

    //tree->debugPrint(); cout<<endl;
    cout<<"Passed basic tests."<<endl;
    
    return 0;
}
