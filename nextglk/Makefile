# Makefile for NextGlk library (derived from CheapGlk)
#
# Two targets:
#   make          - Linux build with cc
#   make specnext - ZX Spectrum Next build with z88dk/zcc

# Pick a C compiler for Linux
CC = cc
OPTIONS = -g -Wall

CFLAGS = $(OPTIONS) $(INCLUDEDIRS)

GLKLIB = libcheapglk.a

CHEAPGLK_OBJS =  \
  cgfref.o cggestal.o cgmisc.o cgstream.o cgstyle.o cgwindow.o cgschan.o \
  cgdate.o cgunicod.o main.o gi_dispa.o gi_blorb.o gi_debug.o cgblorb.o

CHEAPGLK_HEADERS = cheapglk.h gi_dispa.h gi_debug.h

all: $(GLKLIB) Make.cheapglk

cgunicod.o: cgunigen.c

$(GLKLIB): $(CHEAPGLK_OBJS)
	ar r $(GLKLIB) $(CHEAPGLK_OBJS)
	ranlib $(GLKLIB)

Make.cheapglk:
	echo LINKLIBS = $(LIBDIRS) $(LIBS) > Make.cheapglk
	echo GLKLIB = -lcheapglk >> Make.cheapglk

$(CHEAPGLK_OBJS): glk.h $(CHEAPGLK_HEADERS)

clean:
	rm -f *~ *.o $(GLKLIB) Make.cheapglk

# Spectrum Next target using z88dk
# Compiles each source to .o, then links into a binary.
# Uses -x to signal that we're producing an executable.
SPECNEXT_CFLAGS = +zxn -O2 -D__SPECTRUM_NEXT__

specnext: $(CHEAPGLK_OBJS:.o=_next.o)
	@echo "=== Linking NextGlk for ZX Spectrum Next ==="
	zcc $(SPECNEXT_CFLAGS) -x -o gitn.bin $(CHEAPGLK_OBJS:.o=_next.o)
	@echo "=== Spectrum Next build complete ==="

%_next.o: %.c glk.h $(CHEAPGLK_HEADERS) cgunigen.c
	zcc $(SPECNEXT_CFLAGS) -c -o $@ $<
