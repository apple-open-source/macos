#	$Id: Makefile,v 1.1.1.3 2001/06/09 00:28:41 zarzycki Exp $

.ifmake !clean
.include "../../config.int"
.endif

SINGLEKLD?=yes

SUBDIR=	dummy
.if ${SINGLEKLD} == yes
SUBDIR+= smbfs2
.else
SUBDIR+= libiconv netsmb smbfs
.endif

load deinstall install-lib deinstall-lib install-src deinstall-src: _SUBDIRUSE

.include <bsd.subdir.mk>
