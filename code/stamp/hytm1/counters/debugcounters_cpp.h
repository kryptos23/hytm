/* 
 * File:   debugcounters_print.h
 * Author: trbot
 *
 * Created on June 21, 2016, 10:04 PM
 */

#ifndef DEBUGCOUNTERS_PRINT_H
#define	DEBUGCOUNTERS_PRINT_H

#include <string>
#include <sstream>
#include <iostream>
#include "../platform.h"
using namespace std;

#include "debugcounter_impl.h"
#include "debugcounters_impl.h"

string PATH_NAMES[] = {"fast htm", "slow htm", "fallback"};

string getAutomaticAbortNames(const int compressedStatus) {
    stringstream ss;
    if (compressedStatus & _XABORT_EXPLICIT) ss<<" explicit";
    if (compressedStatus & _XABORT_RETRY) ss<<" retry";
    if (compressedStatus & _XABORT_CONFLICT) ss<<" conflict";
    if (compressedStatus & _XABORT_CAPACITY) ss<<" capacity";
    if (compressedStatus & _XABORT_DEBUG) ss<<" __debug";
    if (compressedStatus & _XABORT_NESTED) ss<<" nested";
    return ss.str();
}

string getExplicitAbortName(const int compressedStatus) {
    int explicitCode = getCompressedStatusExplicitAbortCode(compressedStatus);
    if (explicitCode == ABORT_PROCESS_ON_FALLBACK) return "process_on_fallback";
    if (explicitCode >= 42) return "ASSERTION_FAILED";
    return "";
}

string getAllAbortNames(const int compressedStatus) {
    stringstream ss;
    ss<<getExplicitAbortName(compressedStatus)<<getAutomaticAbortNames(compressedStatus);
    return ss.str();
}

void countersPrint(struct debugCounters *cs) {
    for (int path=0;path<NUMBER_OF_PATHS;++path) {
        switch (path) {
            case PATH_FAST_HTM: cout<<"[" << PATH_NAMES[path] << "]" << endl; break;
            case PATH_FALLBACK: cout<<"[" << PATH_NAMES[path] << "]" << endl; break;
        }
        if (counterGetTotal(counters->htmCommit[path])) {
            cout<<"total "<<PATH_NAMES[path]<<" commit         : "<<counterGetTotal(counters->htmCommit[path])<<endl;
#ifdef RECORD_ABORTS
            long long totalAborts = 0;
            for (int i=0;i<MAX_ABORT_STATUS;++i) {
                int _total = counterGetTotal(counters->htmAbort[path*MAX_ABORT_STATUS+i]);
                if (_total) {
                    cout<<"    "<<PATH_NAMES[path]<<" abort            : "<<(i==0?"unexplained":getAllAbortNames(i))<<" "<<_total<<endl;
                    totalAborts += _total;
                }
            }
            cout<<"total "<<PATH_NAMES[path]<<" abort          : "<<totalAborts<<endl;
#endif
        }
    }
//    cout<<"scaled time on fallback (nanos over all processes) : "<<(counterGetTotal(counters->timingOnFallback))<<endl;
    cout<<"seconds global lock is held   : "<<(counterGetTotal(counters->timingOnFallback)/1000000000.)<<endl;
}

#endif	/* DEBUGCOUNTERS_PRINT_H */

