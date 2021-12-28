obj = treestore.o text.o dynarr.o
lib = ../unix/libtreestore.a

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = treestore.w32.o text.w32.o dynarr.w32.o
	lib = ../w32/libtreestore.a
endif
ifeq ($(sys), android-arm64)
	obj = treestore.arm64.o text.arm64.o dynarr.arm64.o
	lib = ../android/libtreestore.a
endif

CFLAGS = -O3

$(lib): $(obj)
	$(AR) rcs $@ $(obj)

%.arm64.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(obj) $(lib)
