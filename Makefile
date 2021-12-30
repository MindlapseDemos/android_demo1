src = $(wildcard src/*.c) $(wildcard src/scr/*.c) $(wildcard src/pc/*.c) libs/glew/glew.c
obj = $(src:.c=.o)
dep = $(src:.c=.d)
bin = demo

warn = -pedantic -Wall
dbg = -g
#opt = -O3 -ffast-math -fno-strict-aliasing
def = -DMINIGLUT_USE_LIBC -DGLEW_STATIC
incdir = -Isrc -Ilibs -Ilibs/imago/src -Ilibs/treestore -Ilibs/glew
libs_path = libs/unix
libdir = -L$(libs_path)

libs = $(libs_path)/libimago.a $(libs_path)/libtreestore.a $(libs_path)/libanim.a \
	   $(libs_path)/libpsys.a

CFLAGS = $(warn) $(dbg) $(opt) $(def) $(incdir) -fcommon -MMD
LDFLAGS = $(libdir) $(libsys) $(libgl) -limago -lpsys -lanim -ltreestore $(libc)

sys ?= $(shell uname -s | sed 's/MINGW.*/mingw/')
ifeq ($(sys), mingw)
	obj = $(src:.c=.w32.o)
	bin = demo.exe
	libgl = -lopengl32
	libsys = -lmingw32 -lgdi32 -lwinmm -mconsole
	libs_path = libs/w32
	libc = -lm
else
	libgl = -lGL -lX11 -lXext
	libc = -lm -ldl
endif

$(bin): $(obj) Makefile $(libs)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.w32.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

$(libs_path)/%.a:
	$(MAKE) -C libs

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

.PHONY: android
android:
	$(MAKE) -f Makefile.android

.PHONY: android-clean
android-clean:
	$(MAKE) -f Makefile.android clean

.PHONY: android-libs
android-libs:
	$(MAKE) -f Makefile.android libs

.PHONY: install
install:
	$(MAKE) -f Makefile.android install

.PHONY: run
run:
	$(MAKE) -f Makefile.android install run

.PHONY: stop
stop:
	$(MAKE) -f Makefile.android stop

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
