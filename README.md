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

Hacking
-------
To add a new "screen", add one or more source files under `src/scr`. Each screen
should define a function called `regscr_<name>` which calls `dsys_add_screen`
with a pointer to a private `demoscreen` structure, to register the screen with
the demosystem. The screen structure contains the screen name, and a number of
function pointers for screen callbacks (See `src/demosys.h`). At least the
`init` and `draw` callbacks need to be non-null.

Loading data files need to use the functions in `assfile.h`, because on android
the data files are in the apk, not on the filesystem. Functions to load textures
and shaders are provided in `assman.h`.

Only the subset of OpenGL calls which are part of OpenGL ES 2.0 should be used.
There are helper functions in `sanegl.h` to bring back some of the missing
functionality, namely immediate mode rendering (with quads), and a matrix
stack.
