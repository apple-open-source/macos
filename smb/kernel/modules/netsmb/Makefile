# $Id: Makefile,v 1.1.1.2 2001/06/09 00:28:41 zarzycki Exp $

.include "../../../config.int"

.PATH:	${.CURDIR}/../../netsmb ${.CURDIR}/../../mysys/libkern

KMOD=	netsmb

SRCS=	smb_conn.c smb_dev.c smb_trantcp.c smb_smb.c smb_subr.c smb_rq.c \
	smb_usr.c smb_crypt.c smb_lock.c \
	subr_mbuf.c tree.c \
	opt_inet.h opt_ipx.h opt_vmpage.h vnode_if.h

# Build with IPX support (1|0)
SMB_IPX?=	1

# Build with INET support (1|0)
SMB_INET?=	1


NOMAN=
KLDMOD=true

CLEANFILES= opt_inet.h opt_inet.h opt_vmpage.h .depend

CFLAGS+= -I${.CURDIR}/../../ -I${SYSDIR}

CFLAGS+= ${KDEBUG}

KERNLINK=${.CURDIR}/../../sys

@ machine ${KERNLINK}:
	rm -f @ machine ${KERNLINK}
	ln -s ${SYSDIR} @
	ln -s ${SYSDIR} ${KERNLINK}
	ln -s ${SYSDIR}/i386/include machine
	echo ${.PATH}

opt_vmpage.h:
	touch ${.TARGET}

opt_inet.h:
	touch ${.TARGET}
.if ${SMB_INET} > 0
	echo "#define INET 1" > ${.TARGET}
.endif

opt_ipx.h:
	touch ${.TARGET}
.if ${SMB_IPX} > 0
	echo "#define IPX 1" > ${.TARGET}
.endif


load: ${KMOD}.ko
	kldload ./${KMOD}.ko

unload:
	@(if kldunload ${KMOD}; then true; else true; fi)

deinstall:
	rm -f /modules/${KMOD}.ko

.include <bsd.kmod.mk>
