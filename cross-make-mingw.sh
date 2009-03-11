#!/bin/sh

COMPILE_PLATFORM=`uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]'`

if [ "$COMPILE_PLATFORM" = "darwin" ]
then
export CC=i386-mingw32msvc-gcc
export WINDRES=i386-mingw32msvc-windres
export PLATFORM=mingw32
exec make $*
exit
fi

if [ "$COMPILE_PLATFORM" = "freebsd" ]
then
export CC=mingw32-gcc
export WINDRES=mingw32-windres
export PLATFORM=mingw32
exec gmake $*
exit
fi

# Default
export CC=i586-mingw32msvc-gcc
export WINDRES=i586-mingw32msvc-windres
export PLATFORM=mingw32
exec make $*
