# $FreeBSD: /repoman/r/ncvs/src/usr.bin/units/Makefile,v 1.8 2001/12/17 16:02:15 ru Exp $

PROG=	units
FILES=	units.lib
FILESDIR=	${SHAREDIR}/misc
TEXTMODE?=  444

# A more modern bsd make system would let one do .include <bsd.files.mk>
beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	  ${.CURDIR}/${FILES} ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
