CC ?= cc

CFLAGS += -Wall -Wextra -std=c99

OBJS = \
	nextglk.o \
	nextglk_screen.o \
	nextglk_input.o \
	nextglk_file.o \
	nextglk_image.o \
	nextglk_platform.o \
	next_stream.o \
	next_window.o

all: libnextglk.a

libnextglk.a: $(OBJS)
	ar rcs $@ $(OBJS)

test: libnextglk.a
	$(CC) $(CFLAGS) -I. -o test_stream ../tests/test_stream_main.c \
		../tests/stream_tests.c -L. -lnextglk -lm
	./test_stream

test_window: libnextglk.a
	$(CC) $(CFLAGS) -I. -o test_window ../tests/test_window_main.c \
		../tests/window_tests.c -L. -lnextglk -lm
	./test_window

test_all: libnextglk.a
	$(CC) $(CFLAGS) -I. -o test_all ../tests/test_main.c \
		../tests/stream_tests.c ../tests/window_tests.c \
		-L. -lnextglk -lm
	./test_all

clean:
	rm -f *.o *.a test_stream test_window test_all
