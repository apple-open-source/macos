# $FreeBSD: src/share/mklocale/Makefile,v 1.28.2.3 2002/01/08 19:58:56 phantom Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=        bg_BG.CP1251 \
		ja_JP.EUC \
		ja_JP.SJIS \
		ko_KR.EUC \
		la_LN.US-ASCII \
		la_LN.ISO8859-1 \
		la_LN.ISO8859-2 \
		la_LN.ISO8859-4 \
		la_LN.ISO8859-15 \
		ru_RU.CP866 \
		ru_RU.ISO8859-5 \
		ru_RU.KOI8-R \
		uk_UA.KOI8-U \
		zh_CN.EUC \
		zh_TW.Big5

LOCALEDIR=	${DESTDIR}/usr/share/locale

ASCIILINKS=	en_AU en_CA en_GB en_US
LATIN1LINKS = \
	da_DK de_AT de_CH de_DE en_AU en_CA en_GB en_US es_ES fi_FI \
	fr_BE fr_CA fr_CH fr_FR is_IS it_CH it_IT nl_BE nl_NL no_NO \
	pt_PT sv_SE

LATIN2LINKS = cs_CZ hr_HR hu_HU pl_PL sl_SI

LATIN4LINKS = lt_LT

.SUFFIXES: .src .out

.src.out:
	mklocale -o ${.TARGET} ${.IMPSRC}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for locale in ${LOCALES}
	mkdir -p ${LOCALEDIR}/${locale}
	${INSTALL} ${COPY} -m 644 -o ${BINOWN} -g ${BINGRP} \
		${locale}.out ${LOCALEDIR}/${locale}/LC_CTYPE
.endfor
.for link in ${LATIN1LINKS}
	mkdir -p ${LOCALEDIR}/${link}.ISO8859-1
	ln -fs ../la_LN.ISO8859-1/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO8859-1/LC_CTYPE
	ln -fs ${link}.ISO8859-1  ${LOCALEDIR}/${link}
.endfor
.for link in ${LATIN1LINKS}
	mkdir -p ${LOCALEDIR}/${link}.ISO8859-15
	ln -fs ../la_LN.ISO8859-15/LC_CTYPE \
		       ${LOCALEDIR}/${link}.ISO8859-15/LC_CTYPE
.endfor
.for link in ${LATIN2LINKS}
	mkdir -p ${LOCALEDIR}/${link}.ISO8859-2
	ln -fs ../la_LN.ISO8859-2/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO8859-2/LC_CTYPE
.endfor
.for link in ${LATIN4LINKS}
	mkdir -p ${LOCALEDIR}/${link}.ISO8859-4
	ln -fs ../la_LN.ISO8859-4/LC_CTYPE \
		${LOCALEDIR}/${link}.ISO8859-4/LC_CTYPE
.endfor
.for link in ${ASCIILINKS}
	mkdir -p ${LOCALEDIR}/${link}.US-ASCII
	ln -fs ../la_LN.US-ASCII/LC_CTYPE \
		${LOCALEDIR}/${link}.US-ASCII/LC_CTYPE
.endfor

.include <bsd.prog.mk>
