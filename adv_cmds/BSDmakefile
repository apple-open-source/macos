NO_WERROR = 1
SUBDIR=cap_mkdb.tproj colldef.tproj finger.tproj fingerd.tproj gencat.tproj last.tproj locale localedef lsvfs.tproj md.tproj mklocale.tproj ps.tproj stty.tproj tabs.tproj tty.tproj usr-share-locale.tproj whois.tproj

INSTALLDIRS= ${DESTDIR}/bin ${DESTDIR}/usr/bin ${DESTDIR}/usr/share/man/man1 ${DESTDIR}/usr/share/man/man5 ${DESTDIR}/usr/share/man/man8 
# If we could change gid before running mkdir we could remove these
INSTALLDIRS += ${DESTDIR}/usr/share ${DESTDIR}/usr/share/man ${DESTDIR}/usr/share/locale

installsrc: clean
	cp -rp . ${SRCROOT}

installhdrs:
	true

beforeinstall:
	-mkdir -p ${INSTALLDIRS}
	-chgrp wheel ${INSTALLDIRS}

.include <bsd.subdir.mk>
