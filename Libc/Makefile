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
# RC_TARGET_CONFIG may not be set, so default to MacOSX (which is good enough
# for installsrc to autopatch all files).
.ifndef RC_TARGET_CONFIG
RC_TARGET_CONFIG = MacOSX
.endif

#use default compiler
#CC = gcc-4.0
GCC_VERSION != cc -dumpversion | sed -e 's/^\([^.]*\.[^.]*\).*/\1/'
GCC_42 != perl -e "print ($(GCC_VERSION) >= 4.2 ? 'YES' : 'NO')"

.ifdef ALTLIBCHEADERS
INCLUDEDIR = ${ALTLIBCHEADERS}
LIBCFLAGS += -I${INCLUDEDIR}
.else # !ALTLIBCHEADERS
INCLUDEDIR = ${SDKROOT}/usr/include
.endif # ALTLIBCHEADERS
FRAMEWORKS = ${OBJROOT}/Frameworks
PRIVATEHEADERS = ${FRAMEWORKS}/System.framework/PrivateHeaders
PRIVINC = -I${PRIVATEHEADERS}
LIBCFLAGS += ${PRIVINC}

SYMROOTINC = ${SYMROOT}/include
CFLAGS = -g -arch ${CCARCH} ${RC_NONARCH_CFLAGS} -std=gnu99 -fno-common -Wmost
CFLAGS += -D__LIBC__ -D__DARWIN_UNIX03=1 -D__DARWIN_64_BIT_INO_T=1 -D__DARWIN_NON_CANCELABLE=1 -D__DARWIN_VERS_1050=1
CFLAGS += -DNOID -DLIBC_MAJOR=${SHLIB_MAJOR}
CFLAGS += -I${.OBJDIR} -I${SYMROOTINC} -I${.CURDIR}/include
AINC = -g -arch ${CCARCH} ${RC_NONARCH_CFLAGS}
AINC += -I${.CURDIR}/${MACHINE_ARCH} ${PRIVINC}
.if $(MACHINE_ARCH) != arm
CFLAGS += -force_cpusubtype_ALL
AINC += -force_cpusubtype_ALL
.endif
.ifdef SDKROOT 
CFLAGS += -isysroot '${SDKROOT}'
AINC += -isysroot '${SDKROOT}'
.endif # SDKROOT

.if ${GCC_42} != YES
CFLAGS += -no-cpp-precomp
AINC += -no-cpp-precomp
.endif
CLEANFILES+=tags
INSTALL_PIC_ARCHIVE=	yes
PRECIOUSLIB=	yes

# workaround for 3649783
AINC += -fdollars-in-identifiers

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
	${SDKROOT}/Developer/Makefiles/bin/version.pl Libc > $@

.include "Makefile.features"
.include "${.CURDIR}/Makefile.inc"
.include "Makefile.xbs"

MANFILTER = unifdef -t ${UNIFDEFARGS}
.if exists(/usr/share/mk/bsd.init.mk)
.include <bsd.init.mk>
.endif
.include <bsd.man.mk>
