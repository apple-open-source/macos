# $FreeBSD: src/share/timedef/Makefile,v 1.30 2002/10/25 22:02:10 ache Exp $

NOMAN=YES
CLEANFILES+= ${LOCALES:S/$/.out/g}

LOCALES=	bg_BG.UTF-8 \
		ca_ES.UTF-8 \
		cs_CZ.UTF-8 \
		da_DK.UTF-8 \
		de_AT.UTF-8 \
		de_DE.UTF-8 \
		el_GR.UTF-8 \
		es_ES.UTF-8 \
		et_EE.UTF-8 \
		fi_FI.UTF-8 \
		fr_FR.UTF-8 \
		hr_HR.UTF-8 \
		hu_HU.UTF-8 \
		is_IS.UTF-8 \
		ja_JP.UTF-8 \
		ko_KR.UTF-8 \
		lt_LT.UTF-8 \
		no_NO.UTF-8 \
		pl_PL.UTF-8 \
		pt_BR.UTF-8 \
		pt_PT.UTF-8 \
		ro_RO.UTF-8 \
		ru_RU.UTF-8 \
		sk_SK.UTF-8 \
		sl_SI.UTF-8 \
		sr_YU.UTF-8 \
		sv_SE.UTF-8 \
		tr_TR.UTF-8 \
		uk_UA.UTF-8 \
		zh_CN.UTF-8 \
		zh_TW.UTF-8

LOCALEDIR=	${DESTDIR}/usr/share/locale

UTF8_LATIN1LINKS=	en_GB en_US it_IT nl_NL
DE_LINKS=	de_CH
FR_LINKS=	fr_BE fr_CA fr_CH
IT_LINKS=	it_CH
LN_LINKS=	la_LN.US-ASCII la_LN.ISO8859-2 la_LN.ISO8859-4
NL_LINKS=	nl_BE
US_LINKS=	en_CA
GB_LINKS=	en_AU en_NZ

.SUFFIXES: .src .out

.src.out:
	grep -v '^#' < ${.IMPSRC} > ${.TARGET}

all: ${LOCALES:S/$/.out/g}

afterinstall:
	for l in ${LOCALES}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/$$l; \
		${INSTALL} -m 644 -o ${BINOWN} -g ${BINGRP} $$l.out \
			   ${LOCALEDIR}/$$l/LC_TIME; \
		u=`basename $$l .UTF-8`; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
			   ${LOCALEDIR}/$$u; \
		ln -fs ../$$l/LC_TIME \
		       ${LOCALEDIR}/$$u/LC_TIME; \
	done
	for l in ${UTF8_LATIN1LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../$$l.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../$$l.ISO8859-1/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${DE_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../de_DE.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../de_DE.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${IT_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../it_IT.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../it_IT.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${FR_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../fr_FR.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../fr_FR.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${NL_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../nl_NL.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../nl_NL.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${US_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../en_US.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../en_US.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	for l in ${GB_LINKS}; do \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l.UTF-8; \
		ln -fs ../en_GB.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l.UTF-8/LC_TIME; \
		${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
		       ${LOCALEDIR}/$$l; \
		ln -fs ../en_GB.UTF-8/LC_TIME \
		       ${LOCALEDIR}/$$l/LC_TIME; \
	done
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/af_ZA.UTF-8
	ln -fs ../en_US.UTF-8/LC_TIME \
	       ${LOCALEDIR}/af_ZA.UTF-8/LC_TIME
	${INSTALL} -d -m 755 -o ${BINOWN} -g ${BINGRP} \
	       ${LOCALEDIR}/af_ZA
	ln -fs ../en_US.UTF-8/LC_TIME \
	       ${LOCALEDIR}/af_ZA/LC_TIME
	rm -f *.out

.include <bsd.prog.mk>
