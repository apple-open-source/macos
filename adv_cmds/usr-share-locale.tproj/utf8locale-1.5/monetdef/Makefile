# $FreeBSD: src/share/monetdef/Makefile,v 1.42 2002/10/25 22:02:09 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	bg_BG.UTF-8 \
		cs_CZ.UTF-8 \
		en_AU.UTF-8 \
		en_GB.UTF-8 \
		pl_PL.UTF-8 \
		ru_RU.UTF-8 \
		sr_YU.UTF-8 \
		uk_UA.UTF-8 \
		zh_CN.UTF-8

LOCALEDIR=	${DESTDIR}/usr/share/locale

UTF8_LATIN1LINKS=	af_ZA ca_ES da_DK de_AT de_CH de_DE en_CA en_NZ en_US \
			es_ES fi_FI fr_BE fr_CA fr_FR is_IS it_IT nl_BE nl_NL \
			no_NO pt_BR pt_PT sv_SE
UTF8_LATIN2LINKS=	hr_HR hu_HU ro_RO sk_SK sl_SI sr_YU
UTF8_LATIN7LINKS=	el_GR
UTF8_LATIN9LINKS=	tr_TR
UTF8_LATIN13LINKS=	lt_LT
UTF8_LATIN15LINKS=	et_EE

CH_SRC=		de_CH
CH_LINKS=	fr_CH it_CH

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${lang}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
		${LOCALEDIR}/${lang}/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${lang:S/.UTF-8$//}
	ln -sf ../${lang}/LC_MONETARY \
		${LOCALEDIR}/${lang:S/.UTF-8$//}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN1LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-1/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN2LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-2/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-2/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN7LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-7/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-7/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN9LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-9/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-9/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN13LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-13/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-13/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${UTF8_LATIN15LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-15/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-15/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
.for link in ${CH_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${CH_SRC}.UTF-8/LC_MONETARY \
		${LOCALEDIR}/${link}.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../${CH_SRC}.UTF-8/LC_MONETARY \
		${LOCALEDIR}/${link}/LC_MONETARY
.endfor
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ja_JP.UTF-8
	ln -sf ../ja_JP.eucJP/LC_MONETARY \
		${LOCALEDIR}/ja_JP.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ja_JP
	ln -sf ../ja_JP.eucJP/LC_MONETARY \
		${LOCALEDIR}/ja_JP/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ko_KR.UTF-8
	ln -sf ../ko_KR.eucKR/LC_MONETARY \
		${LOCALEDIR}/ko_KR.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ko_KR
	ln -sf ../ko_KR.eucKR/LC_MONETARY \
		${LOCALEDIR}/ko_KR/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/zh_TW.UTF-8
	ln -sf ../zh_TW.Big5/LC_MONETARY \
		${LOCALEDIR}/zh_TW.UTF-8/LC_MONETARY
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/zh_TW
	ln -sf ../zh_TW.Big5/LC_MONETARY \
		${LOCALEDIR}/zh_TW/LC_MONETARY
	rm -f *.out

.include <bsd.prog.mk>
