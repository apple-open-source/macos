# $FreeBSD: src/share/numericdef/Makefile,v 1.41 2002/10/25 22:02:10 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	#

LOCALEDIR=	${DESTDIR}/usr/share/locale

UTF8_LATIN1LINKS=	af_ZA ca_ES da_DK de_DE en_US es_ES fi_FI fr_BE fr_FR \
			is_IS it_IT nl_NL no_NO pt_BR pt_PT sv_SE
UTF8_LATIN2LINKS=	cs_CZ hr_HR hu_HU pl_PL ro_RO sk_SK sl_SI
UTF8_LATIN5LINKS=	sr_YU uk_UA
UTF8_LATIN7LINKS=	el_GR
UTF8_LATIN9LINKS=	tr_TR
UTF8_LATIN13LINKS=	lt_LT
UTF8_LATIN15LINKS=	et_EE

US_LINKS=	en_CA en_GB
GB_LINKS=	en_AU en_NZ
FR_LINKS=	fr_CA fr_CH
DE_LINKS=	de_AT de_CH
IT_LINKS=	it_CH

BE_LINKS=	nl_BE

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: #${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/${lang}
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/${lang:S/.UTF-8$//}
	ln -sf ../${lang}/LC_NUMERIC \
			   ${LOCALEDIR}/${lang:S/.UTF-8$//}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN1LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-1/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN2LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-2/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-2/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN5LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-5/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-5/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN7LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-7/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-7/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN9LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-9/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-9/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN13LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-13/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-13/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${UTF8_LATIN15LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8
	ln -sf ../${link}.ISO8859-15/LC_NUMERIC \
	       ${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}
	ln -sf ../${link}.ISO8859-15/LC_NUMERIC \
	       ${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${US_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../en_US.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../en_US.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${GB_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../en_GB.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../en_GB.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${FR_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../fr_FR.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../fr_FR.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${DE_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../de_DE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../de_DE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${IT_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../it_IT.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../it_IT.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
.for link in ${BE_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8
	ln -sf ../fr_BE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}
	ln -sf ../fr_BE.UTF-8/LC_NUMERIC \
		${LOCALEDIR}/${link}/LC_NUMERIC
.endfor
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/bg_BG.UTF-8
	ln -sf ../bg_BG.CP1251/LC_NUMERIC \
	       ${LOCALEDIR}/bg_BG.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/bg_BG
	ln -sf ../bg_BG.CP1251/LC_NUMERIC \
	       ${LOCALEDIR}/bg_BG/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ja_JP.UTF-8
	ln -sf ../ja_JP.eucJP/LC_NUMERIC \
	       ${LOCALEDIR}/ja_JP.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ja_JP
	ln -sf ../ja_JP.eucJP/LC_NUMERIC \
	       ${LOCALEDIR}/ja_JP/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ko_KR.UTF-8
	ln -sf ../ko_KR.eucKR/LC_NUMERIC \
	       ${LOCALEDIR}/ko_KR.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ko_KR
	ln -sf ../ko_KR.eucKR/LC_NUMERIC \
	       ${LOCALEDIR}/ko_KR/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ru_RU.UTF-8
	ln -sf ../ru_RU.CP866/LC_NUMERIC \
	       ${LOCALEDIR}/ru_RU.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/ru_RU
	ln -sf ../ru_RU.CP866/LC_NUMERIC \
	       ${LOCALEDIR}/ru_RU/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/zh_CN.UTF-8
	ln -sf ../zh_CN.eucCN/LC_NUMERIC \
	       ${LOCALEDIR}/zh_CN.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/zh_CN
	ln -sf ../zh_CN.eucCN/LC_NUMERIC \
	       ${LOCALEDIR}/zh_CN/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/zh_TW.UTF-8
	ln -sf ../zh_TW.Big5/LC_NUMERIC \
	       ${LOCALEDIR}/zh_TW.UTF-8/LC_NUMERIC
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/zh_TW
	ln -sf ../zh_TW.Big5/LC_NUMERIC \
	       ${LOCALEDIR}/zh_TW/LC_NUMERIC
	rm -f *.out

.include <bsd.prog.mk>
