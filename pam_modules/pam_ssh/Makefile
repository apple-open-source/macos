# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.6 2001/08/04 21:51:14 markm Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
LDADD=		-L${.OBJDIR}/../../../../secure/lib/libssh -lssh -lcrypto \
		-lcrypt -lutil -lz

.include <bsd.lib.mk>
