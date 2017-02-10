/* 
 * File:   suspended_data.cpp
 * Author: trbot
 *
 * Created on February 8, 2017, 9:41 PM
 */

#include <cstdlib>
#include <iostream>
#include "../hytm1/platform.h"

using namespace std;

volatile int x = 0;
volatile char padding[512];
volatile int y = 0;

int main(int argc, char** argv) {
    XBEGIN_ARG_T arg;
    if (XBEGIN(arg)) {
        x = 1;
        XSUSPEND();
        SYNC;
        y = x;
        SYNC;
        XRESUME();
        XEND();
    } else {
        cout<<"ABORTED"<<endl;
    }

    cout<<"x="<<x<<endl;
    cout<<"y="<<y<<endl; // on power8, we get "ABORTED x=0 y=1" which is pretty interesting. :)
    return 0;
}

