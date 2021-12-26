obj = anim.o track.o dynarr.o
lib = ../unix/libanim.a

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = anim.w32.o track.w32.o dynarr.w32.o
	lib = ../w32/libanim.a
endif
ifeq ($(sys), android-arm64)
	obj = anim.arm64.o track.arm64.o dynarr.arm64.o
	lib = ../android/libanim.a
endif

CFLAGS = -O3 -ffast-math -fno-strict-aliasing -I..

$(lib): $(obj)

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

%.arm64.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
