/**
 * Preliminary C++ implementation of binary search tree using LLX/SCX and DEBRA(+).
 * 
 * Copyright (C) 2015 Trevor Brown
 * This preliminary implementation is CONFIDENTIAL and may not be distributed.
 */

#ifndef MACHINECONSTANTS_H
#define	MACHINECONSTANTS_H

#ifndef MAX_TID_POW2
    #define MAX_TID_POW2 128 // MUST BE A POWER OF TWO, since this is used for some bitwise operations
#endif
#ifndef PHYSICAL_PROCESSORS
    #define PHYSICAL_PROCESSORS 8
#endif

// the following definition is only used to pad data to avoid false sharing.
// although the number of words per cache line is actually 8, we inflate this
// figure to counteract the effects of prefetching multiple adjacent cache lines.
#define PREFETCH_SIZE_WORDS 16
#define PREFETCH_SIZE_BYTES 192
#define BYTES_IN_CACHE_LINE 64
    
// set this to if(1) if you want verbose status messages
#ifndef VERBOSE
    #define VERBOSE if(0)
#endif

#endif	/* MACHINECONSTANTS_H */

