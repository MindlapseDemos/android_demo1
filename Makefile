src = $(wildcard src/*.c) $(wildcard src/pc/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = demo

warn = -pedantic -Wall
dbg = -g
#opt = -O3 -ffast-math -fno-strict-aliasing
def = -DMINIGLUT_USE_LIBC
incdir = -Isrc

CFLAGS = $(warn) $(dbg) $(opt) $(def) $(incdir) -fcommon -MMD
LDFLAGS = $(libsys) $(libgl) -lm

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = $(src:.c=.w32.o)
	bin = demo.exe
	libgl = -lopengl32
	libsys = -lmingw32 -lgdi32 -lwinmm -mconsole
else
	libgl = -lGL -lX11 -lXext
endif

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)

.PHONY: libs
libs:
	$(MAKE) -C libs

.PHONY: clean-libs
clean-libs:
	$(MAKE) -C libs clean


.PHONY: cross
cross:
	$(MAKE) CC=i686-w64-mingw32-gcc sys=mingw

.PHONY: cross-libs
cross-libs:
	$(MAKE) CC=i686-w64-mingw32-gcc sys=mingw -C libs

.PHONY: cross-clean
cross-clean:
	$(MAKE) CC=i686-w64-mingw32-gcc sys=mingw clean

.PHONY: cross-clean-libs
cross-clean-libs:
	$(MAKE) CC=i686-w64-mingw32-gcc sys=mingw -C libs clean
