# ==============================================================================
#
# Defines.common.mk
#
# ==============================================================================


EXTRAFLAGS := $(EXTRAFLAGS1) $(EXTRAFLAGS2) $(EXTRAFLAGS3) $(EXTRAFLAGS4)
CC       := gcc
CFLAGS   += -g -Wall -pthread  $(EXTRAFLAGS)
CFLAGS   += -O3
CFLAGS   += -I$(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS) $(EXTRAFLAGS)
LD       := g++
LIBS     += -lpthread
#ODIR     := ../$(TARGET)
LDFLAGS +=  $(EXTRAFLAGS)

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib

LOSTM := ../../OpenTM/lostm

#OBJ_DIR = $(ODIR)/$(PROG)_objs

STM=../$(TARGET)

# ADD GPROF INSTRUMENTATION
#CPPFLAGS += -pg -Winline
#CFLAGS += -pg -Winline
#LDFLAGS += -pg -Winline

# ==============================================================================
#
# End of Defines.common.mk
#
# ==============================================================================
