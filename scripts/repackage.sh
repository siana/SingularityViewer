#!/bin/bash
#(C) 2011 SIANA GEARZ

usage() {
	echo "Usage: repackage PLATFORM FILEIN.tar.bz2 [FILEOUT.tar.bz2]
Repackage an archive from llautobuild format into singularity format

PLATFORM can be one of windows, linux, linux64, mac.
"
	exit 0
}

TMP="/tmp/pak$$"
LIBPATH=""
INCPATH=""
BINPATH=""

PWD=`pwd`

if [ -z "$1" ]; then
	usage
fi

shopt -s nocasematch
case "$1" in
	--windows|-w|windows|win)
		MODE=win
		LIBPATH="libraries/i686-win32/lib/release"
		LIBDPATH="libraries/i686-win32/lib/debug"
		INCPATH="libraries/i686-win32/include"
		BINPATH="libraries/i686-win32/bin"
		;;
	--mac|--osx|--darwin|-x|mac|osx|darwin)
		MODE=osx
		LIBPATH="libraries/universal-darwin/lib/release"
		LIBDPATH="libraries/universal-darwin/lib/debug"
		INCPATH="libraries/universal-darwin/include"
		BINPATH="libraries/universal-darwin/bin"
		;;
	--lin|--linux|-l|linux)
		MODE=linux
		LIBPATH="libraries/i686-linux/lib/release"
		LIBDPATH="libraries/i686-linux/lib/debug"
		INCPATH="libraries/i686-linux/include"
		BINPATH="libraries/i686-linux/bin"
		;;
	--linux64|-6|linux64)
		MODE=linux64
		LIBPATH="libraries/x86_64-linux/lib/release"
		LIBDPATH="libraries/x86_64-linux/lib/debug"
		INCPATH="libraries/x86_64-linux/include"
		BINPATH="libraries/x86_64-linux/bin"
		;;
	*)
		echo ERROR: No mode specified
		usage
		;;
esac

case "$2" in
	--commoninclude|--common-include|--common)
		echo "Using common include directory"
		INCPATH="libraries/include"
		shift
		;;
esac

FILEIN=$2
if [ -z "$FILEIN" ]; then
	echo ERROR: No input file specified
	usage
fi

test -n "$3" && FILEOUT=`readlink -f $3`
if [ -z "$FILEOUT" ]; then
	FILEOUT=`readlink -m package.tar.bz2`
fi

mkdir "$TMP"

case "$FILEIN" in
	http\:\/\/*|https\:\/\/*)
		echo "	Downloading..."
		cd "$TMP"
		wget "$FILEIN" -O package.tar.bz2
		echo "	Unpacking..."
		tar -xjvf package.tar.bz2
		rm package.tar.bz2
		;;
	*)
		FILEIN=`readlink -e $FILEIN`
		if [ -z "$FILEIN"  ]; then
			echo ERROR: Input file not found
			usage
		fi
		echo "	Unpacking..."
		cd "$TMP"
		tar -xjvf "$FILEIN"
		;;
esac

if [ -n "$LIBDPATH" -a -d lib/debug ]; then
	mkdir -p $LIBDPATH
	mv -f lib/debug/* $LIBDPATH
fi
if [ -d lib/release ]; then
	mkdir -p $LIBPATH
	mv -f lib/release/* $LIBPATH
fi
if [ -d include ]; then
	mkdir -p $INCPATH
	mv -f include/* $INCPATH
fi
if [ -d bin ]; then
	mkdir -p $BINPATH
	mv -f bin/* $BINPATH
fi

echo "	Packing..."
tar -cjvf "$FILEOUT" libraries LICENSES
echo "	Checksum:"
cd "`dirname "$FILEOUT"`"
md5sum -b "`basename "$FILEOUT"`"
cd "$PWD"
rm -rf "$TMP"

