# $FreeBSD: src/share/colldef/Makefile,v 1.52 2002/10/25 22:02:09 ache Exp $
#

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	la_LN.UTF-8

LOCALEDIR=	${DESTDIR}/usr/share/locale

UTF8LINKS=	af_ZA bg_BG ca_ES cs_CZ da_DK de_AT de_CH de_DE el_GR en_AU \
		en_CA en_GB en_NZ en_US es_ES et_EE fi_FI fr_BE fr_CA fr_CH \
		fr_FR hr_HR hu_HU is_IS it_CH it_IT ja_JP ko_KR lt_LT nl_BE \
		nl_NL no_NO pl_PL pt_BR pt_PT ro_RO ru_RU sk_SK sl_SI sr_YU \
		sv_SE tr_TR uk_UA zh_CN zh_TW

.SUFFIXES: .src .out

.src.out:
	colldef -I ${.CURDIR} -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${locale}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_COLLATE
.endfor
.for link in ${UTF8LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -fs ../la_LN.UTF-8/LC_COLLATE \
	       ${LOCALEDIR}/${link}.UTF-8/LC_COLLATE
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -fs ../la_LN.UTF-8/LC_COLLATE \
	       ${LOCALEDIR}/${link}/LC_COLLATE
.endfor
	rm -f *.out

.include <bsd.prog.mk>
