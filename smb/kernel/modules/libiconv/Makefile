# $Id: Makefile,v 1.1.1.3 2001/07/06 22:38:39 conrad Exp $

.ifmake !clean && !cleandepend
.include "../../../config.int"
.endif

.PATH:	${.CURDIR}/../../mysys/libkern

KMOD=	libiconv

#SRCS=	iconv.c iconv_xlat.c iconv_ccs.c iconv_uc.c \
#	iconv_ces.c iconv_ces_table.c iconv_test.c \
#	opt_iconv.h ${CONV_IF} ${CES_IF}
#	iconv_drv_if.c iconv_drv_if.h

#MFILE= ${.CURDIR}/../../mysys/libkern/iconv_drv_if.m

NOMAN=

CLEANFILES= .depend

CFLAGS+= -I${.CURDIR}/../../ -I${SYSDIR} 

CFLAGS+= ${KDEBUG}

KERNLINK=${.CURDIR}/../../sys

@ machine ${KERNLINK}:
	rm -f @ machine ${KERNLINK}
	ln -s ${SYSDIR} @
	ln -s ${SYSDIR}/i386/include machine

unload:
	@(if kldunload ${KMOD}; then true; else true; fi)

deinstall:
	rm -f /modules/${KMOD}.ko

.include <Makefile.iconv>

.include <bsd.kmod.mk>
