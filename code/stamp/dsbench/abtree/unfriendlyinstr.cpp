/* 
 * File:   unfriendlyinstr.cpp
 * Author: trbot
 *
 * Created on November 11, 2015, 7:00 PM
 */

#include <cstdlib>
//#include <immintrin.h>
#include "../common/rtm.h"
#include <iostream>

using namespace std;

//#define XBEGIN() _XBEGIN_STARTED
//#define XEND() 

/*
 * 
 */
int main(int argc, char** argv) {
    int x = 0;
    for (int i=0;i<1000000;++i) {
        int status = XBEGIN();
        if (status == _XBEGIN_STARTED) {
            x = x + 1;
            if (x%2) XABORT(42);
            XEND();
        }
    }
    return x;
}

//int main(int argc, char** argv) {
//    int x = 0;
//    int *z = NULL;
//    for (int i=0;i<1000000;++i) {
//        int status = XBEGIN();
//        if (status == _XBEGIN_STARTED) {
//            x = *z;
//            XEND();
//        }
//    }
//    return x;
//}

//int main(int argc, char** argv) {
//    int n = 10000000;
//    int y = 0;
//    for (int i=0;i<n;++i) {
//        int status = XBEGIN();
//        if (status == _XBEGIN_STARTED) {
//            (&y)[i/10000] = 1;
//            XEND();
//        }
//    }
//    return y;
//}

