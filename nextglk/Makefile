CC ?= cc

CFLAGS += -Wall -Wextra -std=c99

OBJS = \
	nextglk.o \
	nextglk_screen.o \
	nextglk_input.o \
	nextglk_file.o \
	nextglk_image.o \
	nextglk_platform.o

all: libnextglk.a

libnextglk.a: $(OBJS)
	ar rcs $@ $(OBJS)

clean:
	rm -f *.o *.a