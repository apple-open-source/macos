#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/gmon/Makefile.inc,v 1.8 2001/03/27 17:26:47 ru Exp $

# gmon sources
.PATH: ${.CURDIR}/gmon

MISRCS+= gmon.c 

.if ${LIB} == "c"
MAN3 += moncontrol.3

MLINKS += moncontrol.3 monstartup.3
.endif

# mcount cannot be compiled with profiling
gmon.po: 
	${CC} -O -pipe -arch ${MACHINE_ARCH} -Wmost -g -fno-common \
		-no-cpp-precomp -force_cpusubtype_ALL \
		-I${.CURDIR}/include \
		-I${INCLUDEDIR} \
		-I${INCLUDEDIR}/objc \
		${PRIVINC} \
		-c ${.CURDIR}/gmon/gmon.c -o gmon.po

#gmon.po: gmon.o
#	cp gmon.o gmon.po
