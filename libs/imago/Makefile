src = $(wildcard src/*.c) \
	  $(wildcard zlib/*.c) \
	  $(wildcard libpng/*.c) \
	  $(wildcard jpeglib/*.c)
obj = $(src:.c=.o)
alib = ../unix/libimago.a

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = $(src:.c=.w32.o)
	alib = ../w32/libimago.a
endif
ifeq ($(sys), android-arm64)
	obj = $(src:.c=.arm64.o)
	alib = ../android/libimago.a
endif

CFLAGS = -O3 -Izlib -Ilibpng -Ijpeglib

$(alib): $(obj)
	$(AR) rcs $@ $(obj)

%.arm64.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(obj) $(alib)
