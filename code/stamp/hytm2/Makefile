# ==============================================================================
#
# Makefile.real
#
# ==============================================================================

LIBNAME=hytm2

# ==============================================================================
# Variables
# ==============================================================================

CC      := g++
CFLAGS  := -g -Wall -Winline -O3
#CFLAGS  += -m32
LD      := g++

LIBHYTM := lib$(LIBNAME).a

OBJS := \
	$(LIBNAME).o \
	tmalloc.o \
	avpair.o \
	vlock.o \
	log.o \
	typelogs.o \
	thread.o \
#

AR      := ar
RANLIB  := ranlib

RM := rm -f


# ==============================================================================
# Rules
# ==============================================================================

.PHONY: default
default: lazy

#.PHONY: clean
#clean: 
#	$(RM) $(LIBHYTM)

#tmalloc.o: tmalloc.c
#	$(CC) $(CFLAGS) -c $< -o $@

#$(LIBHYTM): vlock.cpp vlock.h avpair.cpp avpair.h log.cpp log.h thread.cpp thread.h typelogs.cpp typelogs.h tmalloc.o $(LIBNAME).cpp $(LIBNAME).h
#	$(CC) $(CFLAGS) -c vlock.cpp avpair.cpp log.cpp thread.cpp typelogs.cpp $(LIBNAME).cpp
	
.PHONY: clean
clean:
	$(RM) $(LIBHYTM) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBHYTM): $(OBJS)
	$(AR) cru $@ $^
	$(RANLIB) $@

.PHONY: lazy
lazy: $(LIBHYTM)



# ==============================================================================
# Dependencies
# ==============================================================================

%.o: %.h


# ==============================================================================
#
# End of Makefile.real
#
# ==============================================================================
