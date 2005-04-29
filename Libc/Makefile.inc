# $FreeBSD: src/lMakefile.inc,v 1.7 2001/04/04 18:17:25 tmm Exp $
#
# This file contains make rules that are shared by libc and libc_r.
#
# Define (empty) variables so that make doesn't give substitution
# errors if the included makefiles don't change these:
MDSRCS=
MISRCS=
MDASM=
MIASM=
NOASM=

# SUPPRESSSRCS is used to prevent machine-independent files from being
# built, when a machine-dependent file defines multiple symbols.
# Use MDSRCS to block one file, and SUPPRESSSRCS to block the others.
SUPPRESSSRCS=

# Auto-patching variables
AUTOPATCHHDRS=
AUTOPATCHMAN=
AUTOPATCHSRCS=

# Auto-patch into OBJROOT
_AUTOPATCH: .USE
	@if [ -f ${.ALLSRC}.patch ]; then \
	    echo cp ${.ALLSRC} ${.TARGET}; \
	    cp ${.ALLSRC} ${.TARGET}; \
	    echo patch ${.TARGET} ${.ALLSRC}.patch; \
	    patch ${.TARGET} ${.ALLSRC}.patch; \
	else \
	    echo ln -s ${.ALLSRC} ${.TARGET}; \
	    ln -s ${.ALLSRC} ${.TARGET}; \
	fi
# Auto-patch into SYMROOT
_AUTOPATCHSYM: .USE
	@echo cp ${.ALLSRC} ${.TARGET}; \
	cp ${.ALLSRC} ${.TARGET}; \
	if [ -f ${.ALLSRC}.patch ]; then \
	    echo patch ${.TARGET} ${.ALLSRC}.patch; \
	    patch ${.TARGET} ${.ALLSRC}.patch; \
	fi

#
# If there is a machine dependent makefile, use it:
#
.if exists(${.CURDIR}/${MACHINE_ARCH}/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"
.endif

.include "${.CURDIR}/darwin/Makefile.inc"
.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/emulated/Makefile.inc"
.include "${.CURDIR}/gdtoa/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/gmon/Makefile.inc"
.include "${.CURDIR}/include/Makefile.inc"
.include "${.CURDIR}/internat/Makefile.inc"
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/mach/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/nls/Makefile.inc"
.include "${.CURDIR}/posix1e/Makefile.inc"
.include "${.CURDIR}/pthreads/Makefile.inc"
.if !defined(NO_QUAD)
#.include "${.CURDIR}/quad/Makefile.inc"
.endif
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/stdtime/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/threads/Makefile.inc"
.include "${.CURDIR}/util/Makefile.inc"
.include "${.CURDIR}/uuid/Makefile.inc"
#.include "${.CURDIR}/rpc/Makefile.inc"
#.include "${.CURDIR}/xdr/Makefile.inc"
#.if !defined(NO_YP_LIBC)
#CFLAGS+= -DYP
#.include "${.CURDIR}/yp/Makefile.inc"
#.endif
#.if !defined(NO_HESIOD_LIBC)
#CFLAGS+= -DHESIOD
#.endif

# If there are no machine dependent sources, append all the
# machine-independent sources:
.if empty(MDSRCS)
SRCS+=	${MISRCS}
.else
# Append machine-dependent sources, then append machine-independent sources
# for which there is no machine-dependent variant, and not being suppressed.
SRCS+=	${MDSRCS}
_SUPPRESS= ${MDSRCS} ${SUPPRESSSRCS}
.for _src in ${MISRCS}
.if ${_SUPPRESS:R:M${_src:R}} == ""
SRCS+=	${_src}
.endif
.endfor
.endif
