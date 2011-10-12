#!/bin/bash

#CONFOPT="--disable-xquartz --disable-glx --disable-dri --disable-launchd --enable-kdrive --disable-xsdl --enable-xnest --enable-xvfb"

CONFOPT="--enable-standalone-xpbproxy"
#CONFOPT="--disable-shave --without-dtrace"

CONFOPT="${CONFOPT} --with-dtrace"

# Parallel Make.  Change $MAKE if you don't have gmake installed
MAKE="gnumake"
MAKE_OPTS="-j10"

. ~/src/strip.sh

PREFIX=/usr/X11
ARCHFLAGS="-arch i386 -arch x86_64"

#PREFIX=/opt/X11
#CONFOPT="$CONFOPT --with-apple-application-name=XQuartz --with-launchd-id-prefix=org.macosforge.xquartz"
#ARCHFLAGS="-arch i386 -arch x86_64"

ACLOCAL="aclocal -I ${PREFIX}/share/aclocal -I /usr/local/share/aclocal"

CPPFLAGS="-DNO_ALLOCA"

CFLAGS="$CFLAGS -Os -ggdb3 -pipe"
CFLAGS="$CFLAGS $ARCHFLAGS"
CFLAGS="$CFLAGS -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers"

    TB_CFLAGS="-fdiagnostics-show-category=name"

# Stage 1:
    TB_CFLAGS="${TB_CFLAGS} -Werror=implicit"
    TB_CFLAGS="${TB_CFLAGS} -Werror=nonnull"
    TB_CFLAGS="${TB_CFLAGS} -Wformat-security"         # <rdar://problem/9418512> clang is overzealous about -Werror=format-*
    TB_CFLAGS="${TB_CFLAGS} -Wformat-extra-args"
    TB_CFLAGS="${TB_CFLAGS} -Wformat-y2k"
    TB_CFLAGS="${TB_CFLAGS} -Werror=init-self"
    TB_CFLAGS="${TB_CFLAGS} -Werror=main"
    TB_CFLAGS="${TB_CFLAGS} -Werror=missing-braces"
    TB_CFLAGS="${TB_CFLAGS} -Wparentheses"             # libX11 XKBBind.c:169
    TB_CFLAGS="${TB_CFLAGS} -Werror=sequence-point"
    TB_CFLAGS="${TB_CFLAGS} -Werror=return-type"
    TB_CFLAGS="${TB_CFLAGS} -Werror=trigraphs"
    TB_CFLAGS="${TB_CFLAGS} -Werror=array-bounds"
#    TB_CFLAGS="${TB_CFLAGS} -Wcast-align"             # Noisy
    TB_CFLAGS="${TB_CFLAGS} -Werror=write-strings"
#    TB_CFLAGS="${TB_CFLAGS} -Werror=clobbered"
    TB_CFLAGS="${TB_CFLAGS} -Werror=address"
    TB_CFLAGS="${TB_CFLAGS} -Werror=int-to-pointer-cast"
    TB_CFLAGS="${TB_CFLAGS} -Werror=pointer-to-int-cast"

# Stage 2:
#    TB_CFLAGS="${TB_CFLAGS} -Wlogical-op"
    TB_CFLAGS="${TB_CFLAGS} -Wunused"
    TB_CFLAGS="${TB_CFLAGS} -Wuninitialized"
    TB_CFLAGS="${TB_CFLAGS} -Wshadow"
#    TB_CFLAGS="${TB_CFLAGS} -Wunsafe-loop-optimizations"
    TB_CFLAGS="${TB_CFLAGS} -Wcast-qual"
    TB_CFLAGS="${TB_CFLAGS} -Wmissing-noreturn"
    TB_CFLAGS="${TB_CFLAGS} -Wmissing-format-attribute"
    TB_CFLAGS="${TB_CFLAGS} -Wredundant-decls"
    TB_CFLAGS="${TB_CFLAGS} -Wnested-externs"
    TB_CFLAGS="${TB_CFLAGS} -Winline"

OBJCFLAGS="$CFLAGS"
LDFLAGS="$CFLAGS"

#CC="llvm-gcc"
#CXX="llvm-g++"
CC="clang"
CXX="clang++"

OBJC="$CC"

#SCAN_BUILD="scan-build -v -V -o clang.d --use-cc=${CC} --use-c++=${CXX}"
 
#CPPFLAGS="$CPPFLAGS -F/Applications/Utilities/XQuartz.app/Contents/Frameworks"
#LDFLAGS="$LDFLAGS -F/Applications/Utilities/XQuartz.app/Contents/Frameworks"
#CPPFLAGS="$CPPFLAGS -F/Applications/Utilities/X11.app/Contents/Frameworks"
#LDFLAGS="$LDFLAGS -F/Applications/Utilities/X11.app/Contents/Frameworks"
#CONFOPT="${CONFOPT} --enable-sparkle"

# This section is for building release tarballs
if false ; then
	CONFOPT="${CONFOPT} --enable-docs --enable-devel-docs --enable-builddocs --with-doxygen --with-xmlto --with-fop"
	export XMLTO=/opt/local/bin/xmlto
	export ASCIIDOC=/opt/local/bin/asciidoc
	export DOXYGEN=/opt/local/bin/doxygen
	export FOP=/opt/local/bin/fop
	export FOP_OPTS="-Xmx2048m -Djava.awt.headless=true"
	export GROFF=/opt/local/bin/groff
	export PS2PDF=/opt/local/bin/ps2pdf
fi

export ACLOCAL CPPFLAGS CFLAGS OBJCFLAGS LDFLAGS CC OBJC

PKG_CONFIG_PATH=${PREFIX}/share/pkgconfig:${PREFIX}/lib/pkgconfig:$PKG_CONFIG_PATH
PATH=${PREFIX}/bin:$PATH

die() {
	echo "${@}" >&2
	exit 1
}

docomp() {
	autoreconf -fvi || die
	${SCAN_BUILD} ./configure --prefix=${PREFIX} ${CONFOPT} --disable-dependency-tracking --enable-maintainer-mode --enable-xcsecurity --enable-record --disable-xevie "${@}" || die "Could not configure xserver"
	${MAKE} clean || die "Unable to make clean"
	${SCAN_BUILD} ${MAKE} ${MAKE_OPTS} || die "Could not make xserver"
}

doinst() {
	${MAKE} install DESTDIR="$(pwd)/../dist" || die "Could not install xserver"
}

dosign() {
	/opt/local/bin/gmd5sum $1 > $1.md5sum
	/opt/local/bin/gsha1sum $1 > $1.sha1sum
	DISPLAY="" /opt/local/bin/gpg2 -b $1
}

dodist() {
	${MAKE} dist
	dosign xorg-server-$1.tar.bz2

	cp hw/xquartz/mach-startup/X11.bin X11.bin-$1
	bzip2 X11.bin-$1
	dosign X11.bin-$1.bz2 
}

docomp `[ -f conf_flags ] && cat conf_flags`
#doinst
[[ -n $1 ]] && dodist $1

exit 0
