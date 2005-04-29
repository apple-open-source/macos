# $Id: Makefile,v 1.2 2004/09/17 20:41:59 lindak Exp $

.ifmake !clean && !cleandepend
.include "../../../config.int"
.endif


.PATH:	${.CURDIR}/../../fs/smbfs \
	${.CURDIR}/../../netsmb ${.CURDIR}/../../mysys/libkern \
	${SYSDIR}/crypto/des

KMOD=	smbfs

SRCS=	vnode_if.h \
	opt_inet.h opt_ipx.h \
	opt_netsmb.h opt_smbfs.h opt_vmpage.h \
	smb_compat4.c md4c.c \
	smb_conn.c smb_dev.c smb_trantcp.c smb_smb.c smb_subr.c smb_rq.c \
	smb_usr.c smb_crypt.c smb_iod.c \
	smbfs_vfsops.c smbfs_node.c smbfs_io.c smbfs_vnops.c \
	smbfs_subr.c smbfs_smb.c
#	iconv.c iconv_xlat.c \
#	opt_nls.h iconv_converter_if.c iconv_converter_if.h \

.if defined(ENCRYPTED_PASSWD)
SRCS+=	des_ecb.c des_setkey.c
.endif

MFILE= ${.CURDIR}/../../mysys/libkern/iconv_drv_if.m
MCFILE=${.CURDIR}/../../mysys/libkern/iconv_converter_if.m

# Build with IPX support (1|0)
SMB_IPX?=	0

# Build with INET support (1|0)
SMB_INET?=	1

NOMAN=true

SYS5=${.CURDIR}/../../sys5
MYSYS=${.CURDIR}/../../mysys

CFLAGS+= -I${.CURDIR}/../../ -I${SYSDIR}

CFLAGS+= ${KDEBUG}

CLEANFILES=  .depend
#opt_inet.h opt_inet.h opt_vmpage.h

.if !exists(${SYSDIR}/sys/mchain.h)
SRCS+=	subr_mchain.c
.endif

@ machine ${KERNLINK}:
	rm -f @ machine ${KERNLINK}
	ln -s ${SYSDIR} @
	ln -s ${SYSDIR}/i386/include machine

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT -DWITNESS
.endif

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

opt_netsmb.h:
	touch ${.TARGET}
.if defined(ENCRYPTED_PASSWD)
	echo "#define NETSMBCRYPTO 1" >> ${.TARGET}
.endif

#opt_nls.h:
#	touch ${.TARGET}
#	echo "#define NLS 1" > ${.TARGET}

#iconv_drv_if.h iconv_drv_if.c: ${MFILE}
#	perl ${MAKEOBJOPS} -c -h ${MFILE}

#iconv_converter_if.h iconv_converter_if.c: ${MCFILE}
#	perl ${MAKEOBJOPS} -c -h ${MCFILE}

unload:
	@(if kldunload ${KMOD}; then true; else true; fi)

deinstall:
	rm -f /modules/${KMOD}.ko

.include "../libiconv/Makefile.iconv"

.include <bsd.kmod.mk>
