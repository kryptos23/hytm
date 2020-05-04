/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef MACHINECONSTANTS_H
#define	MACHINECONSTANTS_H

#ifndef MAX_TID_POW2
    #define MAX_TID_POW2 512 // MUST BE A POWER OF TWO, since this is used for some bitwise operations
#endif
#ifndef PHYSICAL_PROCESSORS
    #define PHYSICAL_PROCESSORS 512
#endif

// the following definition is only used to pad data to avoid false sharing.
// although the number of words per cache line is actually 8, we inflate this
// figure to counteract the effects of prefetching multiple adjacent cache lines.
#define PREFETCH_SIZE_WORDS 24
#define PREFETCH_SIZE_BYTES 192
#define BYTES_IN_CACHE_LINE 128
    
// set this to if(1) if you want verbose status messages
#ifndef VERBOSE
    #define VERBOSE if(0)
#endif

#define CAT2(x, y) x##y
#define CAT(x, y) CAT2(x, y)
#define PAD64 volatile char CAT(___padding, __COUNTER__)[64]
#define PAD volatile char CAT(___padding, __COUNTER__)[PREFETCH_SIZE_BYTES]

#endif	/* MACHINECONSTANTS_H */

