/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef LOCKFREESTACK_H
#define	LOCKFREESTACK_H

#include <atomic>
#include <iostream>
#include "blockbag.h"
using namespace std;

// lock free bag that operates on elements of the block<T> type,
// defined in blockbag.h. this class does NOT allocate or deallocate any memory.
// instead, it simply chains blocks together using their next pointers.
// the implementation is a stack, with push and pop at the head.
// the aba problem is avoided using version numbers with a double-wide CAS.
// any contention issues with using a simple stack and overhead issues with
// double-wide CAS are unimportant, because operations on this bag only happen
// once a process has filled up two blocks of objects and needs to hand one
// off. thus, the number of operations on this class is several orders of
// magnitude smaller than the number of operations on the binary search tree.
template <typename T>
class lockfreeblockbag {
private:
    PAD;
    struct tagged_ptr {
        block<T> *ptr;
        long tag;
    };
    std::atomic<tagged_ptr> head;
    PAD;
public:
    lockfreeblockbag() {
        VERBOSE DS_DEBUG cout<<"constructor lockfreeblockbag lockfree="<<head.is_lock_free()<<endl;
        assert(head.is_lock_free());
        head.store(tagged_ptr({NULL,0}));
    }
    ~lockfreeblockbag() {
        VERBOSE DS_DEBUG cout<<"destructor lockfreeblockbag; ";
        block<T> *curr = head.load(memory_order_relaxed).ptr;
        int debugFreed = 0;
        while (curr) {
            block<T> * const temp = curr;
            curr = curr->next;
            DS_DEBUG ++debugFreed;
            delete temp;
        }
        VERBOSE DS_DEBUG cout<<"freed "<<debugFreed<<endl;
    }
    block<T>* getBlock() {
        while (true) {
            tagged_ptr expHead = head.load(memory_order_relaxed);
            if (expHead.ptr != NULL) {
                if (head.compare_exchange_weak(
                        expHead,
                        tagged_ptr({expHead.ptr->next, expHead.tag+1}))) {
                    block<T> *result = expHead.ptr;
                    result->next = NULL;
                    return result;
                }
            } else {
                return NULL;
            }
        }
    }
    void addBlock(block<T> *b) {
        while (true) {
            tagged_ptr expHead = head.load(memory_order_relaxed);
            b->next = expHead.ptr;
            if (head.compare_exchange_weak(
                    expHead,
                    tagged_ptr({b, expHead.tag+1}))) {
                return;
            }
        }
    }
    // NOT thread safe
    int sizeInBlocks() {
        int result = 0;
        block<T> *curr = head.load(memory_order_relaxed).ptr;
        while (curr) {
            ++result;
            curr = curr->next;
        }
        return result;
    }
};

#endif	/* LOCKFREESTACK_H */
