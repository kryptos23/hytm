# ==============================================================================
#
# Makefile.real
#
# ==============================================================================

LIBNAME=hytm2

# ==============================================================================
# Variables
# ==============================================================================

EXTRAFLAGS := $(EXTRAFLAGS1) $(EXTRAFLAGS2) $(EXTRAFLAGS3) $(EXTRAFLAGS4)
CC       := g++
CFLAGS   := -g -Wall -Winline -O3 $(EXTRAFLAGS)
CPP      := g++
CPPFLAGS := -g -Wall -Winline -O3 $(EXTRAFLAGS) -DRECORD_ABORTS
LD       := g++
LDFLAGS  += $(EXTRAFLAGS)

LIBHYTM  := lib$(LIBNAME).a

AR       := ar
RANLIB   := ranlib

RM := rm -f

# ADD GPROF INSTRUMENTATION
#CPPFLAGS += -pg
#CFLAGS += -pg
#LDFLAGS += -pg -Winline

# ==============================================================================
# Rules
# ==============================================================================

.PHONY: default
default: lazy

.PHONY: clean
clean:
	$(RM) $(LIBHYTM) $(LIBNAME).o tmalloc.o

$(LIBHYTM).o: $(LIBHYTM).cpp
	$(CC) $(CFLAGS) -c $< -o $@

tmalloc.o: tmalloc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBHYTM): $(LIBNAME).o tmalloc.o
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
