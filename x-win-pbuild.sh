#!/bin/bash
# this script creates a windows32 version of xjadeo
# cross-compiled on GNU/Linux
#
# It is intended to run in a pristine chroot or VM of a minimal
# debian system. see http://wiki.debian.org/cowbuilder
#

: ${SRC=/usr/src}
: ${SRCDIR=/tmp/winsrc}
: ${PREFIX=$SRC/win-stack}
: ${BUILDD=$SRC/win-build}
: ${MAKEFLAGS=-j4}

if [ "$(id -u)" != "0" -a -z "$SUDO" ]; then
	echo "This script must be run as root in pbuilder" 1>&2
  echo "e.g sudo cowbuilder --architecture amd64 --distribution wheezy --bindmounts /tmp --execute $0"
	exit 1
fi

apt-get -y install build-essential \
	gcc-mingw-w64-i686 g++-mingw-w64-i686 mingw-w64-tools mingw32 \
	wget git autoconf automake libtool pkg-config \
	curl unzip ed yasm \
	nsis

cd "$SRC"
git clone -b master --single-branch git://github.com/x42/xjadeo.git

set -e

###############################################################################

mkdir -p ${SRCDIR}
mkdir -p ${PREFIX}
mkdir -p ${BUILDD}

unset PKG_CONFIG_PATH
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export PREFIX
export SRCDIR

function download {
echo "--- Downloading.. $2"
test -f ${SRCDIR}/$1 || curl -k -L -o ${SRCDIR}/$1 $2
}

function autoconfbuild {
echo "======= $(pwd) ======="
PATH=${PREFIX}/bin:/usr/bin:/bin:/usr/sbin:/sbin \
	CPPFLAGS="-I${PREFIX}/include" \
	CFLAGS="-I${PREFIX}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	CXXFLAGS="-I${PREFIX}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	LDFLAGS="-L${PREFIX}/lib" \
	./configure --host=i686-w64-mingw32 --build=i386-linux --prefix=$PREFIX --enable-shared $@
  make $MAKEFLAGS && make install
}

################################################################################
download jack_headers.tar.gz http://robin.linuxaudio.org/jack_headers.tar.gz
cd "$PREFIX"
tar xzf ${SRCDIR}/jack_headers.tar.gz
"$PREFIX"/update_pc_prefix.sh

################################################################################
download pthreads-w32-2-9-1-release.tar.gz ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/pthreads-w32-2-9-1-release.tar.gz
cd pthreads-w32-2-9-1-release
make clean GC CROSS=i686-w64-mingw32-
mkdir -p ${PREFIX}/bin
mkdir -p ${PREFIX}/lib
mkdir -p ${PREFIX}/include
cp -vf pthreadGC2.dll ${PREFIX}/bin/
cp -vf libpthreadGC2.a ${PREFIX}/lib/libpthread.a
cp -vf pthread.h sched.h ${PREFIX}/include

################################################################################
download zlib-1.2.7.tar.gz ftp://ftp.simplesystems.org/pub/libpng/png/src/history/zlib/zlib-1.2.7.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/zlib-1.2.7.tar.gz
cd zlib-1.2.7
make -fwin32/Makefile.gcc PREFIX=i686-w64-mingw32-
make install -fwin32/Makefile.gcc SHARED_MODE=1 \
	INCLUDE_PATH=${PREFIX}/include \
	LIBRARY_PATH=${PREFIX}/lib \
	BINARY_PATH=${PREFIX}/bin

################################################################################
download libiconv-1.14.tar.gz ftp://ftp.gnu.org/gnu/libiconv/libiconv-1.14.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/libiconv-1.14.tar.gz
cd libiconv-1.14
autoconfbuild --with-included-gettext --with-libiconv-prefix=$PREFIX

################################################################################
#git://liblo.git.sourceforge.net/gitroot/liblo/liblo
download liblo-0.28.tar.gz http://downloads.sourceforge.net/liblo/liblo-0.28.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/liblo-0.28.tar.gz
cd liblo-0.28
PATH=${PREFIX}/bin:/usr/bin:/bin:/usr/sbin:/sbin \
	CPPFLAGS="-I${PREFIX}/include" \
	CFLAGS="-I${PREFIX}/include" \
	CXXFLAGS="-I${PREFIX}/include" \
	LDFLAGS="-L${PREFIX}/lib" \
	./configure --host=i686-w64-mingw32 --build=i386-linux --prefix=$PREFIX --enable-shared $@
ed src/Makefile << EOF
/noinst_PROGRAMS
.,+3d
wq
EOF
ed Makefile << EOF
%s/examples//
wq
EOF
make $MAKEFLAGS && make install


################################################################################
#git://github.com/x42/libltc.git
download libltc-1.1.4.tar.gz https://github.com/x42/libltc/releases/download/v1.1.4/libltc-1.1.4.tar.gz
cd ${BUILDD}
tar zxf ${SRCDIR}/libltc-1.1.4.tar.gz
cd libltc-1.1.4
autoconfbuild

################################################################################
#git clone -b VER-2-5-3 --depth 1  git://git.sv.gnu.org/freetype/freetype2.git
download freetype-2.5.3.tar.gz http://download.savannah.gnu.org/releases/freetype/freetype-2.5.3.tar.gz
cd ${BUILDD}
tar xzf ${SRCDIR}/freetype-2.5.3.tar.gz
cd freetype-2.5.3
autoconfbuild -with-harfbuzz=no --with-png=no

################################################################################

# svn checkout svn://svn.code.sf.net/p/portmedia/code/portmidi/trunk/
download portmidi-src-217.zip http://sourceforge.net/projects/portmedia/files/portmidi/217/portmidi-src-217.zip/download
cd ${BUILDD}
rm -rf portmidi
unzip ${SRCDIR}/portmidi-src-217.zip
cd portmidi
touch AUTHORS ChangeLog README INSTALL COPYING NEWS
echo "SUBDIRS = pm_win" > Makefile.am
cat > pm_win/Makefile.am << EOF
include_HEADERS = ../pm_common/portmidi.h ../porttime/porttime.h
lib_LTLIBRARIES = libporttime.la libportmidi.la

libporttime_la_SOURCES = \
	../porttime/porttime.c \
	../porttime/ptwinmm.c

libporttime_la_CFLAGS = -I ../porttime
libporttime_la_LDFLAGS = -mms-bitfields -mwindows -shared -no-undefined -lwinmm

libportmidi_la_SOURCES = \
	../pm_common/pmutil.c  \
	../pm_common/portmidi.c  \
	pmwin.c  \
	pmwinmm.c

libportmidi_la_CFLAGS = -I ../pm_common -I ../porttime
libportmidi_la_LDFLAGS = -mms-bitfields -mwindows -shared -no-undefined -lwinmm -L.libs -lporttime
EOF
cat > configure.ac << EOF
AC_PREREQ([2.63])
AC_INIT([portmidi], [217])
AC_CONFIG_SRCDIR([pm_win/pmwin.c])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_FILES([Makefile])
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC
AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
AC_CHECK_HEADERS([stdint.h stdlib.h string.h unistd.h])
AC_HEADER_STDBOOL
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_FUNCS([atexit])
AC_OUTPUT([pm_win/Makefile])
EOF
aclocal
autoconf
autoreconf -i
autoconfbuild

################################################################################
download libvpx-1.4.0.tar.bz2 http://downloads.webmproject.org/releases/webm/libvpx-1.4.0.tar.bz2
cd ${BUILDD}
tar xjf ${SRCDIR}/libvpx-1.4.0.tar.bz2
cd libvpx-1.4.0
ed vpx/src/svc_encodeframe.c << EOF
%s/MINGW_HAS_SECURE_API/MINGW_HAS_SECURE_APIXXX/
wq
EOF
CC=i686-w64-mingw32-gcc\
	CPPFLAGS="-I${PREFIX}/include" \
	CFLAGS="-I${PREFIX}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	CXXFLAGS="-I${PREFIX}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	LDFLAGS="-L${PREFIX}/lib" \
	CROSS=i686-w64-mingw32- ./configure --target=x86-win32-gcc --prefix=$PREFIX \
	--disable-examples --disable-docs --disable-install-bins
make -j4 && make install

################################################################################
FFVERSION=2.8.2
download ffmpeg-${FFVERSION}.tar.bz2 http://www.ffmpeg.org/releases/ffmpeg-${FFVERSION}.tar.bz2
cd ${BUILDD}
tar xjf ${SRCDIR}/ffmpeg-${FFVERSION}.tar.bz2
cd ffmpeg-${FFVERSION}/
ed configure << EOF
%s/jack_jack_h/xxjack_jack_h/
%s/enabled jack_indev/enabled xxjack_indev/
%s/sdl_outdev_deps="sdl"/sdl_outdev_deps="xxxsdl"/
%s/enabled sdl/enabled xxsdl/
%s/pkg_config_default="\${cross_prefix}\${pkg_config_default}"/pkg_config_default="\${pkg_config_default}"/
wq
EOF

./configure --prefix=${PREFIX} \
	--disable-programs \
	--enable-gpl --enable-shared --disable-static --disable-debug \
	--enable-libvpx \
	--arch=i686 --target-os=mingw32 --cpu=i686 --enable-cross-compile --cross-prefix=i686-w64-mingw32- \
	--extra-cflags="-I${PREFIX}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64" \
	--extra-ldflags="-L${PREFIX}/lib"
make $MAKEFLAGS && make install


###############################################################################

cd "$SRC"/xjadeo
aclocal
autoconf
autoreconf -i

export WINPREFIX="$PREFIX"

./x-win-bundle.sh
