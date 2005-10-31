#	@(#)Makefile	8.2 (Berkeley) 2/3/94
# $FreeBSD: src/lib/libc/Makefile,v 1.31 2001/08/13 21:48:43 peter Exp $
#
# All library objects contain rcsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_RCS and -DSYSLIBC_RCS
# from CFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_RCS from CFLAGS.
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
CC = gcc-4.0
# always set __DARWIN_UNIX03 to zero (variant will set to one) except for ppc64
.if (${MACHINE_ARCH} == ppc64)
CFLAGS += -D__DARWIN_UNIX03=1
.else
CFLAGS += -D__DARWIN_UNIX03=0
.endif
CFLAGS += -D__LIBC__ -DNOID -I${.CURDIR}/include
.ifdef ALTLIBCHEADERS
INCLUDEDIR = ${ALTLIBCHEADERS}
CFLAGS += -I${INCLUDEDIR}
.endif
.ifdef ALTFRAMEWORKSPATH
PRIVINC = -F${ALTFRAMEWORKSPATH} -I${ALTFRAMEWORKSPATH}/System.framework/PrivateHeaders
.else
PRIVINC = -I${NEXT_ROOT}/System/Library/Frameworks/System.framework/PrivateHeaders
.endif
CFLAGS += ${PRIVINC}
CFLAGS += -DLIBC_MAJOR=${SHLIB_MAJOR} -no-cpp-precomp -force_cpusubtype_ALL
CFLAGS += -fno-common -pipe -Wmost -g -D__FBSDID=__RCSID
CFLAGS += -finline-limit=1500 --param inline-unit-growth=200 -Winline
AINC=	-I${.CURDIR}/${MACHINE_ARCH} -no-cpp-precomp -force_cpusubtype_ALL
AINC+=-arch ${MACHINE_ARCH} -g
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

# workaround for 3649783
AINC += -fdollars-in-identifiers

# ppc64 optimizer still blows up on some files, so we use -O0 to turn it
# off on a per file basis
.if (${MACHINE_ARCH} == ppc64)
OPTIMIZE-acl_entry.c = -O0
# glob-fbsd.c fails with -static -Os (3869444) so turn off optimization
OPTIMIZE-glob-fbsd.c = -O0
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

CFLAGS += -I${SYMROOT}
.include "${.CURDIR}/Makefile.inc"
.PATH: ${SYMROOT}
.include "Makefile.xbs"
.if exists(/usr/share/mk/bsd.init.mk)
.include <bsd.init.mk>
.endif
.include <bsd.man.mk>
