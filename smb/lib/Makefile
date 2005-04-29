# $Id: Makefile,v 1.2 2004/12/13 00:25:19 lindak Exp $

.ifmake !clean && !cleandepend
.include "../config.int"
.endif

#.if defined(COMPLETEBUILD) || !exists(/usr/lib/libnls.a)
#SUBDIR+= nls
#.endif

#.if defined(COMPLETEBUILD) || !exists(/usr/lib/libnb.a)
#SUBDIR+= nb
#.endif

.if defined(COMPLETEBUILD) || !exists(/usr/lib/libsmb.a)
SUBDIR+= smb
.endif

deinstall install-lib deinstall-lib install-src deinstall-src: _SUBDIRUSE

.include <bsd.subdir.mk>
