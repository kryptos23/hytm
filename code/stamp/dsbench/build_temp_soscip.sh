#!/bin/sh

make -f Makefile.stm -j machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20 pinning=SOSCIP_SCATTER

#for alg in tl2 hytm1 hytm2 hytm3 hybridnorec ; do
#	make -f Makefile.stm -j TARGET=$alg machine=`hostname` EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=20 pinning=SOSCIP_SCATTER ;
#done
