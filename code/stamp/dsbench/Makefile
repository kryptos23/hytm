PROG := dsbench

##################################
## SET THESE APPROPRIATELY
##################################
FLAGS += -DPHYSICAL_PROCESSORS=512
FLAGS += -DMAX_TID_POW2=512

FLAGS += -fno-omit-frame-pointer

#FLAGS += -fsanitize=address

### if libpapi is installed but you do not want to use it, invoke make with extra argument "has_libpapi=0"
has_libpapi=$(shell ./_check_lib.sh papi)
ifneq ($(has_libpapi), 0)
    FLAGS += -DUSE_PAPI
    LDFLAGS += -lpapi
endif

EXTRAFLAGS3=-DHTM_ATTEMPT_THRESH=40 ## DEFAULT HTM retry/attempt threshold

FLAGS += -std=c++0x
FLAGS += -O3
# FLAGS += -fopenmp
#FLAGS += -DSTM -I$(STM) -I$(LIB)
FLAGS += -g3 #-mx32
FLAGS += -I./common -I./includes -I../lib
EXTRAFLAGS := $(EXTRAFLAGS1) $(EXTRAFLAGS2) $(EXTRAFLAGS3) $(EXTRAFLAGS4) $(xargs)
FLAGS += $(EXTRAFLAGS)
FLAGS += -DRECORD_ABORTS
FLAGS += -DNDEBUG
FLAGS += -DDS_DEBUG=if\(0\) -DDS_DEBUG1=if\(0\) -DDS_DEBUG2=if\(0\) -DVERBOSE=if\(0\)

LDFLAGS += -lpthread
#LDFLAGS += -l$(TARGET)
#LDFLAGS += -ltcmalloc
#LDFLAGS += -L$(STM)

machine=$(shell hostname)
pinning=IDENTITY

GPP = g++

#all: hytm2_3path hytm2
all: tl2 hytm1 hytm2 hytm3 hybridnorec hytm2_3path add
##hybridnorec hytm2_nonspec hybridnorec_nonspec

bin_dir=bin
dir_guard:
	@mkdir -p $(bin_dir)

.PHONY : all clean hytm2_3path tl2 hytm1 hytm2 hytm3 hybridnorec hytm2_nonspec hybridnorec_nonspec dir_guard add

clean: dir_guard
	rm -f $(bin_dir)/$(machine).$(PROG)_*

tl2: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hytm1: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hytm2: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hytm2_3path: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hytm3: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hybridnorec: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -D$@ -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hytm2_nonspec: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -Dhytm2 -DUSE_SUSPEND_RESUME -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

hybridnorec_nonspec: dir_guard
	$(GPP) $(FLAGS) -o $(bin_dir)/$(machine).$(PROG)_$@ -DALGO="\"$@\"" -Dhybridnorec -DUSE_SUSPEND_RESUME -DTHREAD_BINDING=$(pinning) ./main.cpp ../lib/thread.c $(LDFLAGS)

add: dir_guard
	$(GPP) common/add.cpp -o $(bin_dir)/add
