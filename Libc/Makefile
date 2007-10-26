#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD: src/lib/libc/Makefile,v 1.31 2001/08/13 21:48:43 peter Exp $
#
# Yes, we build everything with -g, and strip it out later...
#
# -faltivec now disables inlining, so we can't use it globally.  Fortunately,
# only two files need altivec support, so we use file-specific CFLAGS to add
# the option when needed.
#
LIB=c
SHLIB_MAJOR= 1
SHLIB_MINOR= 0
.if (${MACHINE_ARCH} == unknown)
MACHINE_ARCH != /usr/bin/arch
.endif 
.if !empty $(MACHINE_ARCH:M*64)
LP64 = 1
.endif
CC = gcc-4.0
CFLAGS += -D__LIBC__ -D__DARWIN_UNIX03=1 -D__DARWIN_64_BIT_INO_T=1 -D__DARWIN_NON_CANCELABLE=1 -D__DARWIN_VERS_1050=1
CFLAGS += -DNOID -I${.CURDIR}/include -std=gnu99
.ifdef ALTLIBCHEADERS
INCLUDEDIR = ${ALTLIBCHEADERS}
LIBCFLAGS += -I${INCLUDEDIR}
.endif
LIBCFLAGS += -I$(SRCROOT)/include -include _.libc_internal.h
FRAMEWORKS = ${OBJROOT}/Frameworks
PRIVATEHEADERS = ${FRAMEWORKS}/System.framework/PrivateHeaders
PRIVINC = -F${FRAMEWORKS} -I${PRIVATEHEADERS}
CFLAGS += ${PRIVINC} -I${.OBJDIR}
CFLAGS += -DLIBC_MAJOR=${SHLIB_MAJOR} -no-cpp-precomp -force_cpusubtype_ALL
CFLAGS += -fno-common -pipe -Wmost -g -D__FBSDID=__RCSID
AINC=	-I${.CURDIR}/${MACHINE_ARCH} -no-cpp-precomp -force_cpusubtype_ALL
AINC+=-arch ${MACHINE_ARCH} -g
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

# workaround for 3649783
AINC += -fdollars-in-identifiers

# workaround for 4268581
.if make(lib${LIB}_static.a)
OPTIMIZE-glob-fbsd.c += -O0
.endif

# If these aren't set give it expected defaults
DSTROOT ?= /
OBJROOT ?= .
SRCROOT ?= ${.CURDIR}
.ifndef SYMROOT
SYMROOT = ${.CURDIR}/SYMROOT
_x_ != test -d ${SYMROOT} || mkdir -p ${SYMROOT}
.endif
DESTDIR ?= ${DSTROOT}
MAKEOBJDIR ?= ${OBJROOT}

# add version string
SRCS += libc_version.c
libc_version.c:
	/Developer/Makefiles/bin/version.pl Libc > $@

.include "${.CURDIR}/Makefile.inc"
.include "Makefile.xbs"
.if exists(/usr/share/mk/bsd.init.mk)
.include <bsd.init.mk>
.endif
.include <bsd.man.mk>
