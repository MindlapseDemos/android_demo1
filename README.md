Untitled android demo
=====================
This is the source readme file. For the demo info file see <name>.nfo

Multiplatform demo for Android, GNU/Linux, and Windows.

Grab data dir from subversion:

    svn co svn://mutantstargoat.com/datadirs/android_demo1 data


License
-------
Copyright (C) 2022 John Tsiombikas <nuclear@member.fsf.org>

This demo is free software. Feel free to use, modify, and/or redistribute it
under the terms of the GNU General Public License version 3, or at your option
any later version published by the Free Software Foundation. See COPYING for
details.


Android build
-------------
Set up the environment with two variables: `SDK` and `NDK` pointing to the
directories where the android SDK and NDK are installed. The default paths
correspond with the locations debian's package manager installs them after
running:

  apt-get install android-sdk android-sdk-platform-23 google-android-ndk-installer

Also the `AVER` variable can be set to something other than 23 to change which
android platform version to use.

  - `make android` to build the apk.
  - `make install` to install to the connected android device.
  - `make run` to install and run on the connected android device.
  - `make stop` to kill the demo forcefully.

The above are "shortcuts" in the main makefile. The real android makefile is
`Makefile.android`. Some operations might have to be performed using that one.
For instance to clean and rebuild all libraries:

    make -f Makefile.android clean-libs
    make -f Makefile.android libs

Or to monitor the android log stream:

    make -f Makefile.android logcat


PC version build
----------------
On UNIX and windows under msys2, just type `make`. No external dependencies are
needed, everything is included in the source tree.

For msys2 builds, start either a "mingw32 console" or a "mingw64 console", to
have the environment set up correctly to build native 32bit or 64bit binaries.
Do not start an "msys2 console"; that's for building with the UNIX system call
emulation layers linked to the binary (similar to old cygwin).

Alternatively msvc2022 project files are included, configured to build for
64bit x86. Warning: msvc project files might be out of date. If you encounter
unresolved symbols, make sure to add any new source files to the project.

Hacking
-------

### Demo screens (parts/subparts)
To add a new "screen", add one or more source files under `src/scr`. Each screen
should define a function called `regscr_<name>` which calls `dsys_add_screen`
with a pointer to a private `demoscreen` structure, to register the screen with
the demosystem. The screen structure contains the screen name, and a number of
function pointers for screen callbacks (See `src/demosys.h`). At least the
`init` and `draw` callbacks need to be non-null.

To override the demoscript and work on a single screen, add the line
`screen = <name>` in the `demo.cfg` file. Events defined in the demoscript will
still be available.

### Asset loading
Loading data files need to use the functions in `assfile.h`, because on android
the data files are in the apk, not on the filesystem. Functions to load textures
and shaders are provided in `assman.h`.

### OpenGL compatibility
Only the subset of OpenGL calls which are part of OpenGL ES 2.0 should be used.
There are helper functions in `sanegl.h` to bring back some of the missing
functionality, namely immediate mode rendering (with quads), and a matrix
stack.
