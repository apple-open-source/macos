# $FreeBSD: src/share/msgdef/Makefile,v 1.43 2002/10/25 22:02:09 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	bg_BG.UTF-8 \
		el_GR.UTF-8 \
		ja_JP.UTF-8 \
		ru_RU.UTF-8 \
		sr_YU.UTF-8 \
		uk_UA.UTF-8 \
		zh_CN.UTF-8 \
		zh_TW.UTF-8

LOCALEDIR=	${DESTDIR}/usr/share/locale

UTF8_LATIN1LINKS=	af_ZA ca_ES da_DK de_DE en_CA en_GB en_US es_ES fi_FI \
			fr_BE fr_CH fr_FR is_IS it_IT nl_NL no_NO pt_PT sv_SE
UTF8_LATIN2LINKS=	cs_CZ hr_HR hu_HU pl_PL ro_RO sl_SI
UTF8_LATIN9LINKS=	tr_TR
UTF8_LATIN13LINKS=	lt_LT
UTF8_LATIN15LINKS=	et_EE

GB_LINKS=	en_AU en_NZ
FR_LINKS=	fr_CA
DE_LINKS=	de_AT de_CH
CZ_LINKS=	sk_SK
IT_LINKS=	it_CH
NL_LINKS=	nl_BE
PT_LINKS=	pt_BR

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
.for lang in ${LOCALES}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/${lang}/LC_MESSAGES
	${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} ${lang}.out \
			   ${LOCALEDIR}/${lang}/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/${lang:S/.UTF-8$//}/LC_MESSAGES
	ln -sf ../../${lang}/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${lang:S/.UTF-8$//}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${UTF8_LATIN1LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-1/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-1/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${UTF8_LATIN2LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-2/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-2/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${UTF8_LATIN9LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-9/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-9/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${UTF8_LATIN13LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-13/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-13/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${UTF8_LATIN15LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-15/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../${link}.ISO8859-15/LC_MESSAGES/LC_MESSAGES \
	       ${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${GB_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../en_GB.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../en_GB.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${FR_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../fr_FR.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../fr_FR.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${DE_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../de_DE.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../de_DE.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${IT_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../it_IT.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../it_IT.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${NL_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../nl_NL.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../nl_NL.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${CZ_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../cs_CZ.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../cs_CZ.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
.for link in ${PT_LINKS}
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES
	ln -sf ../../pt_PT.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/${link}/LC_MESSAGES
	ln -sf ../../pt_PT.UTF-8/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/${link}/LC_MESSAGES/LC_MESSAGES
.endfor
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ko_KR.UTF-8/LC_MESSAGES
	ln -sf ../../ko_KR.eucKR/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/ko_KR.UTF-8/LC_MESSAGES/LC_MESSAGES
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		${LOCALEDIR}/ko_KR/LC_MESSAGES
	ln -sf ../../ko_KR.eucKR/LC_MESSAGES/LC_MESSAGES \
		${LOCALEDIR}/ko_KR/LC_MESSAGES/LC_MESSAGES
	rm -f *.out

.include <bsd.prog.mk>
