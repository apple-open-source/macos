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

# Auto-patch (or symlink)
_AUTOPATCH: .USE
	@if [ -f ${.ALLSRC}.patch ]; then \
	    echo cp ${.ALLSRC} ${.TARGET}; \
	    cp ${.ALLSRC} ${.TARGET}; \
	    echo patch ${.TARGET} ${.ALLSRC}.patch; \
	    patch ${.TARGET} ${.ALLSRC}.patch; \
	else \
	    t=`basename ${.ALLSRC}` && x=`dirname ${.ALLSRC}` && d=`basename $$x`; \
	    echo ln -fs $$d/$$t ${.TARGET}; \
	    ln -fs $$d/$$t ${.TARGET}; \
	fi
# Standard compilation for the various forms
_STANDARD_STATIC: .USE
	${CC} -static \
	    ${PRECFLAGS} ${PRECFLAGS-${.IMPSRC:T}} \
	    ${CFLAGS} ${CFLAGS-${.IMPSRC:T}} \
	    ${VARIANTCFLAGS} ${LIBCFLAGS} \
	    -Os ${OPTIMIZE-${.IMPSRC:T}} -c ${.IMPSRC} -o ${.TARGET}
_STANDARD_PROFILE: .USE
	${CC} -pg -DPROFILE \
	    ${PRECFLAGS} ${PRECFLAGS-${.IMPSRC:T}} \
	    ${CFLAGS} ${CFLAGS-${.IMPSRC:T}} \
	    ${VARIANTCFLAGS} ${LIBCFLAGS} \
	    -Os ${OPTIMIZE-${.IMPSRC:T}} -c ${.IMPSRC} -o ${.TARGET}
_STANDARD_DYNAMIC: .USE
	${CC} \
	    ${PRECFLAGS} ${PRECFLAGS-${.IMPSRC:T}} \
	    ${CFLAGS} ${CFLAGS-${.IMPSRC:T}} \
	    ${VARIANTCFLAGS} ${LIBCFLAGS} \
	    -Os ${OPTIMIZE-${.IMPSRC:T}} -c ${.IMPSRC} -o ${.TARGET}
_STANDARD_DEBUG: .USE
	${CC} -g -DDEBUG \
	    ${PRECFLAGS} ${PRECFLAGS-${.IMPSRC:T}} \
	    ${CFLAGS} ${CFLAGS-${.IMPSRC:T}} \
	    ${VARIANTCFLAGS} ${LIBCFLAGS} \
	    -c ${.IMPSRC} -o ${.TARGET}

#
# If there is a machine dependent makefile, use it:
#
.sinclude "${.CURDIR}/${MACHINE_ARCH}/Makefile.inc"

.include "${.CURDIR}/darwin/Makefile.inc"
.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/compat-43/Makefile.inc"
.include "${.CURDIR}/emulated/Makefile.inc"
.include "${.CURDIR}/gdtoa/Makefile.inc"
.include "${.CURDIR}/gen/Makefile.inc"
.include "${.CURDIR}/gmon/Makefile.inc"
.include "${.CURDIR}/include/Makefile.inc"
.ifdef FEATURE_LEGACY_NX_INTERNAT_APIS
.include "${.CURDIR}/internat/Makefile.inc"
.endif
.include "${.CURDIR}/locale/Makefile.inc"
.include "${.CURDIR}/man/Makefile.inc"
.include "${.CURDIR}/net/Makefile.inc"
.include "${.CURDIR}/nls/Makefile.inc"
.include "${.CURDIR}/posix1e/Makefile.inc"
.include "${.CURDIR}/pthreads/Makefile.inc"
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/secure/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdlib/Makefile.inc"
.include "${.CURDIR}/stdtime/Makefile.inc"
.include "${.CURDIR}/string/Makefile.inc"
.include "${.CURDIR}/sys/Makefile.inc"
.include "${.CURDIR}/threads/Makefile.inc"
.include "${.CURDIR}/util/Makefile.inc"
.include "${.CURDIR}/uuid/Makefile.inc"

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
