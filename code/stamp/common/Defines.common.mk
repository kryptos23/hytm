# ==============================================================================
#
# Defines.common.mk
#
# ==============================================================================


CC       := gcc
CFLAGS   += -g -Wall -pthread
CFLAGS   += -O3
CFLAGS   += -I$(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread
#ODIR     := ../$(TARGET)

# Remove these files when doing clean
OUTPUT +=

LIB := ../lib

LOSTM := ../../OpenTM/lostm

#OBJ_DIR = $(ODIR)/$(PROG)_objs

STM=../$(TARGET)

# ADD GPROF INSTRUMENTATION
CPPFLAGS += -pg
CFLAGS += -pg

# ==============================================================================
#
# End of Defines.common.mk
#
# ==============================================================================
