/* 
 * File:   debugcounters_print.h
 * Author: trbot
 *
 * Created on June 21, 2016, 10:04 PM
 */

#ifndef C_DEBUGCOUNTERS_PRINT_H
#define	C_DEBUGCOUNTERS_PRINT_H

#include <string>
#include <sstream>
#include <iostream>
#include "../platform.h"
using namespace std;

#include "debugcounter_impl.h"
#include "debugcounters_impl.h"

string cpp_getAutomaticAbortNames(const int compressedStatus) {
    stringstream ss;
//    if (compressedStatus & (1<<BIT_USER)) ss<<" explicit";
    if (compressedStatus & (1<<BIT_CAPACITY)) ss<<" capacity";
    if (compressedStatus & (1<<BIT_CONFLICT)) ss<<" conflict";
    if (compressedStatus & (1<<BIT_RETRY)) ss<<" retry";
    if (compressedStatus & (1<<BIT_ILLEGAL)) ss<<" illegal";
    if (compressedStatus & (1<<BIT_OTHER)) ss<<" other(nest,impl,supended)";
    if (compressedStatus & (1<<BIT_RESERVED)) ss<<" reserved";
    if (compressedStatus & (1<<BIT_PRIVILEGED)) ss<<" privileged";
    return ss.str();
//    return "";
}

string cpp_getExplicitAbortName(const int compressedStatus) {
//    int explicitCode = cpp_getCompressedStatusExplicitAbortCode(compressedStatus);
//    if (explicitCode == ABORT_PROCESS_ON_FALLBACK) return "process_on_fallback";
//    if (explicitCode >= 42) return "ASSERTION_FAILED";
//    return "";
    int explicitCode = (compressedStatus>>BIT_USER_NAME_START);
    stringstream ss;
    if (compressedStatus & (1<<BIT_USER)) {
        ss<<"explicit("<<explicitCode<<")";
    }
    return ss.str();
}

string cpp_getAllAbortNames(const int compressedStatus) {
    stringstream ss;
    ss<<cpp_getExplicitAbortName(compressedStatus)<<cpp_getAutomaticAbortNames(compressedStatus);
    return ss.str();
//    return "";
}

void countersPrint(struct c_debugCounters *cs) {
    string PATH_NAMES[] = {"fast htm", "slow htm", "fallback"};
    for (int path=0;path<NUMBER_OF_PATHS;++path) {
        switch (path) {
            case PATH_FAST_HTM: cout<<"[" << PATH_NAMES[path] << "]" << endl; break;
            case PATH_FALLBACK: cout<<"[" << PATH_NAMES[path] << "]" << endl; break;
        }
        if (counterGetTotal(cs->htmCommit[path])) {
            cout<<"total "<<PATH_NAMES[path]<<" commit         : "<<counterGetTotal(cs->htmCommit[path])<<endl;
#ifdef RECORD_ABORTS
            long long totalAborts = 0;
            for (int i=0;i<MAX_ABORT_STATUS;++i) {
                int _total = counterGetTotal(cs->htmAbort[path*MAX_ABORT_STATUS+i]);
                if (_total) {
                    cout<<"    "<<PATH_NAMES[path]<<" abort            : "<<(i==0?"unexplained":cpp_getAllAbortNames(i))<<" "<<_total<<endl;
                    totalAborts += _total;
                }
            }
            cout<<"total "<<PATH_NAMES[path]<<" abort          : "<<totalAborts<<endl;
#endif
        }
    }
//    cout<<"scaled time on fallback (nanos over all processes) : "<<(counterGetTotal(cs->timingOnFallback))<<endl;
    cout<<"seconds global lock is held   : "<<(counterGetTotal(cs->timingOnFallback)/1000000000.)<<endl;
}

#endif	/* DEBUGCOUNTERS_PRINT_H */

