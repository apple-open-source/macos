# $Id: Makefile,v 1.1.1.3 2001/06/09 00:28:41 zarzycki Exp $

.ifmake !clean && !cleandepend
.include "../../../config.int"
.endif

SYS5=${.CURDIR}/../../sys5
MYSYS=${.CURDIR}/../../mysys

KERNLINK=${.CURDIR}/../../sys

.ifmake !clean && !cleandepend
SYSCTL5=\#define SYSCTL_HANDLER_ARGS s
SYSCTLVER!=grep -c "^${SYSCTL5}" < ${SYSDIR}/sys/sysctl.h
.else
SYSCTLVER=0
.endif
SYSCTLCONF=	${MYSYS}/sys/sysctlconf.h

@ machine ${KERNLINK} ${SYSCTLCONF}:
	rm -f @ machine ${KERNLINK}
	ln -s ${SYSDIR} @
	ln -s ${SYSDIR} ${KERNLINK}
	ln -s ${SYSDIR}/i386/include machine
.if !exists(${SYSDIR}/sys/kobj.h)
	ln -s ${SYS5}/sys/kobj.h ${MYSYS}/sys/kobj.h
	ln -s ${SYS5}/kern/subr_kobj.c ${MYSYS}/libkern/subr_kobj.c
.endif
.if !exists(${SYSDIR}/sys/mchain.h)
	ln -s ${SYS5}/sys/mchain.h ${MYSYS}/sys/mchain.h
	ln -s ${SYS5}/kern/subr_mchain.c ${MYSYS}/libkern/subr_mchain.c
.endif
.if ${SYSCTLVER} == 1
	echo "#define	NEW_SYSCTL_ARGS" > ${MYSYS}/sys/sysctlconf.h
.else
	touch ${MYSYS}/sys/sysctlconf.h
.endif

all depend: @


clean:
	rm -f ${MYSYS}/sys/kobj.h ${MYSYS}/libkern/subr_kobj.c @ machine \
	    ${MYSYS}/sys/sysctlconf.h ${KERNLINK} \
	    ${MYSYS}/sys/mchain.h ${MYSYS}/libkern/subr_mchain.c

load:

unload:

install:
deinstall:
cleandepend:

#.include <bsd.kmod.mk>
