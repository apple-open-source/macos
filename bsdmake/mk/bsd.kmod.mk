# $FreeBSD: src/share/mk/bsd.kmod.mk,v 1.85 2000/07/07 05:12:33 imp Exp $

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/) || !exists(${SYSDIR}/conf/)
.error "can't find kernel source tree"
.endif

.include "${SYSDIR}/conf/kmod.mk"
