# $Id: Makefile,v 1.1.1.2 2001/06/09 00:28:41 zarzycki Exp $

.include "../../../config.int"

.PATH:	${.CURDIR}/../../fs/smbfs

KMOD=	smbfs
KMODDEPS= netsmb

SRCS=	vnode_if.h \
	smbfs_vfsops.c smbfs_node.c smbfs_io.c smbfs_vnops.c \
	smbfs_subr.c smbfs_smb.c \
	opt_nsmb.h opt_smbfs.h opt_vmpage.h

NOMAN=true

CFLAGS+= -I${.CURDIR}/../../ -I${SYSDIR}

CFLAGS+= ${KDEBUG}

KERNLINK=${.CURDIR}/../../sys

@ machine ${KERNLINK}:
	rm -f @ machine ${KERNLINK}
	ln -s ${SYSDIR} @
	ln -s ${SYSDIR} ${KERNLINK}
	ln -s ${SYSDIR}/i386/include machine
	echo ${.PATH}


.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

opt_nsmb.h:
	echo "#define NSMB	1"  > ${.TARGET}

load: ${KMOD}.ko
	kldload ./${KMOD}.ko

unload:
	@(if kldunload ${KMOD}; then true; else true; fi)

deinstall:
	rm -f /modules/${KMOD}.ko

.include <bsd.kmod.mk>
