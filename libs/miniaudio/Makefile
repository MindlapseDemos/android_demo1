obj = miniaudio.o
lib = ../unix/libminiaudio.a

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = miniaudio.w32.o
	lib = ../w32/libminiaudio.a
endif
ifeq ($(sys), android-arm64)
	obj = miniaudio.arm64.o
	lib = ../android64/libminiaudio.a
	pic = -fPIC
endif
ifeq ($(sys), android-arm32)
	obj = miniaudio.arm32.o
	lib = ../android32/libminiaudio.a
	pic = -fPIC
endif

CFLAGS = -O3 $(pic)

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

%.arm64.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.arm32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
