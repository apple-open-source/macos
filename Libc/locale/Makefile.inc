#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/locale/Makefile.inc,v 1.43 2003/03/13 06:29:53 tjr Exp $

# locale sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/locale ${.CURDIR}/locale

# depreciated interfaces
MISRCS += frune.c mbrune.c runedepreciated.c setinvalidrune.c

# extended locale
MISRCS += isctype_l.c iswctype_l.c xlocale.c

.include "Makefile.fbsd_begin"
FBSDMISRCS= big5.c btowc.c collate.c collcmp.c euc.c fix_grouping.c \
	gb18030.c gb2312.c gbk.c isctype.c iswctype.c \
	ldpart.c lmessages.c lmonetary.c lnumeric.c localeconv.c \
	mblen.c mbrlen.c mbrtowc.c mbsinit.c mbsnrtowcs.c mbsrtowcs.c \
	mbstowcs.c mbtowc.c mskanji.c \
	nextwctype.c nl_langinfo.c nomacros.c none.c \
	rune.c runetype.c setlocale.c setrunelocale.c \
	table.c tolower.c toupper.c utf8.c \
	wcrtomb.c wcsftime.c wcsnrtombs.c wcsrtombs.c wcstod.c \
	wcstof.c wcstoimax.c wcstol.c wcstold.c wcstoll.c wcstombs.c \
	wcstoul.c wcstoull.c wcstoumax.c wctob.c wctomb.c wctrans.c \
	wctype.c wcwidth.c
FBSDHDRS= collate.h ldpart.h lmessages.h lmonetary.h lnumeric.h \
	mblocal.h setlocale.h
.include "Makefile.fbsd_end"

# special case: utf2-fbsd.c is derived from utf8.c with utf2.c.patch
AUTOPATCHSRCS+= ${SYMROOT}/utf2-fbsd.c
MISRCS+= utf2.c
${SYMROOT}/utf2-fbsd.c: FreeBSD/utf8.c
	cp ${.ALLSRC} ${.TARGET}
	patch ${.TARGET} ${.ALLSRC:S/utf8/utf2/}.patch;

# also build a 64-bit long double version (ppc only)
LDBLSRCS += wcstold.c

# Begin hack for 3333969

MISRCS += lconv.c
CFLAGS-lconv.c += -D__APPLE_PR_3333969_HACK__

# End hack for 3333969

# for ppc64, we need to create rune32.h
.if (${MACHINE_ARCH} == ppc64)
# the following is good enough for ppc, ppc64 and i386
ARCH32 != arch | sed 's/64//'
rune.So rune.do rune.po rune.o: ${SYMROOT}/rune32.h
${SYMROOT}/rune32.h: ${SYMROOT}/rune-fbsd.c
	${CC} -arch ${ARCH32} -DRUNEOFF32 -o ${SYMROOT}/rune32 ${.ALLSRC}
	${SYMROOT}/rune32 > ${.TARGET}
.endif

.if ${LIB} == "c"
MAN3+=	ctype_l.3 isalnum_l.3 iswalnum_l.3 wcstod_l.3 wcstol_l.3
MAN3+=	duplocale.3 freelocale.3 newlocale.3 \
	querylocale.3 uselocale.3 xlocale.3
# depreciated man pages
MAN3+= mbrune.3 rune.3
MAN5+= utf2.5

.include "Makefile.fbsd_begin"
FBSDMAN3= btowc.3 ctype.3 digittoint.3 \
	isalnum.3 isalpha.3 isascii.3 isblank.3 iscntrl.3 isdigit.3 \
	isgraph.3 isideogram.3 islower.3 isphonogram.3 isprint.3 \
	ispunct.3 isrune.3 isspace.3 isspecial.3 isupper.3 \
	iswalnum.3 isxdigit.3 \
	localeconv.3 \
	mblen.3 mbrlen.3 mbrtowc.3 mbsinit.3 mbsrtowcs.3 mbstowcs.3 \
	mbtowc.3 multibyte.3 \
	nextwctype.3 nl_langinfo.3 \
	setlocale.3 toascii.3 tolower.3 toupper.3 towlower.3 towupper.3 \
	wcrtomb.3 wcsftime.3 wcsrtombs.3 wcstod.3 wcstol.3 wcstombs.3 \
	wctomb.3 wctrans.3 wctype.3 wcwidth.3
FBSDMAN5= big5.5 euc.5 gb18030.5 gb2312.5 gbk.5 mskanji.5 utf8.5
.include "Makefile.fbsd_end"

MLINKS+=btowc.3 wctob.3
MLINKS+=btowc.3 btowc_l.3 btowc.3 wctob_l.3
MLINKS+=digittoint.3 digittoint_l.3
MLINKS+=isdigit.3 isnumber.3
MLINKS+=isalnum_l.3 isalpha_l.3 isalnum_l.3 isblank_l.3 \
	isalnum_l.3 iscntrl_l.3 isalnum_l.3 isdigit_l.3 isalnum_l.3 isgraph_l.3 \
	isalnum_l.3 ishexnumber_l.3 \
	isalnum_l.3 isideogram_l.3 isalnum_l.3 islower_l.3 isalnum_l.3 isnumber_l.3 \
	isalnum_l.3 isphonogram_l.3 isalnum_l.3 isprint_l.3 isalnum_l.3 ispunct_l.3 \
	isalnum_l.3 isrune_l.3 isalnum_l.3 isspace_l.3 isalnum_l.3 isspecial_l.3 \
	isalnum_l.3 isupper_l.3 isalnum_l.3 isxdigit_l.3
MLINKS+=iswalnum.3 iswalpha.3 iswalnum.3 iswascii.3 iswalnum.3 iswblank.3 \
	iswalnum.3 iswcntrl.3 iswalnum.3 iswdigit.3 iswalnum.3 iswgraph.3 \
	iswalnum.3 iswhexnumber.3 \
	iswalnum.3 iswideogram.3 iswalnum.3 iswlower.3 iswalnum.3 iswnumber.3 \
	iswalnum.3 iswphonogram.3 iswalnum.3 iswprint.3 iswalnum.3 iswpunct.3 \
	iswalnum.3 iswrune.3 iswalnum.3 iswspace.3 iswalnum.3 iswspecial.3 \
	iswalnum.3 iswupper.3 iswalnum.3 iswxdigit.3
MLINKS+=iswalnum_l.3 iswalpha_l.3 iswalnum_l.3 iswblank_l.3 \
	iswalnum_l.3 iswcntrl_l.3 iswalnum_l.3 iswdigit_l.3 iswalnum_l.3 iswgraph_l.3 \
	iswalnum_l.3 iswhexnumber_l.3 \
	iswalnum_l.3 iswideogram_l.3 iswalnum_l.3 iswlower_l.3 iswalnum_l.3 iswnumber_l.3 \
	iswalnum_l.3 iswphonogram_l.3 iswalnum_l.3 iswprint_l.3 iswalnum_l.3 iswpunct_l.3 \
	iswalnum_l.3 iswrune_l.3 iswalnum_l.3 iswspace_l.3 iswalnum_l.3 iswspecial_l.3 \
	iswalnum_l.3 iswupper_l.3 iswalnum_l.3 iswxdigit_l.3
MLINKS+=isxdigit.3 ishexnumber.3
MLINKS+=localeconv.3 localeconv_l.3
MLINKS+=mblen.3 mblen_l.3
MLINKS+=mbrlen.3 mbrlen_l.3
MLINKS+=mbrtowc.3 mbrtowc_l.3
MLINKS+=mbrune.3 mbmb.3 mbrune.3 mbrrune.3
MLINKS+=mbsinit.3 mbsinit_l.3
MLINKS+=mbsrtowcs.3 mbsnrtowcs.3
MLINKS+=mbsrtowcs.3 mbsrtowcs_l.3 mbsrtowcs.3 mbsnrtowcs_l.3
MLINKS+=mbstowcs.3 mbstowcs_l.3
MLINKS+=mbtowc.3 mbtowc_l.3
MLINKS+=nextwctype.3 nextwctype_l.3
MLINKS+=nl_langinfo.3 nl_langinfo_l.3
MLINKS+=rune.3 fgetrune.3 rune.3 fputrune.3 rune.3 fungetrune.3 \
	rune.3 setinvalidrune.3 rune.3 setrunelocale.3 rune.3 sgetrune.3 \
	rune.3 sputrune.3
MLINKS+=tolower.3 tolower_l.3
MLINKS+=toupper.3 toupper_l.3
MLINKS+=towlower.3 towlower_l.3
MLINKS+=towupper.3 towupper_l.3
MLINKS+=wcrtomb.3 wcrtomb_l.3
MLINKS+=wcsftime.3 wcsftime_l.3
MLINKS+=wcsrtombs.3 wcsnrtombs.3
MLINKS+=wcsrtombs.3 wcsrtombs_l.3 wcsrtombs.3 wcsnrtombs_l.3
MLINKS+=wcstod.3 wcstof.3 wcstod.3 wcstold.3
MLINKS+=wcstod_l.3 wcstof_l.3 wcstod_l.3 wcstold_l.3
MLINKS+=wcstol.3 wcstoul.3 wcstol.3 wcstoll.3 wcstol.3 wcstoull.3 \
	wcstol.3 wcstoimax.3 wcstol.3 wcstoumax.3
MLINKS+=wcstol_l.3 wcstoul_l.3 wcstol_l.3 wcstoll_l.3 wcstol_l.3 wcstoull_l.3 \
	wcstol_l.3 wcstoimax_l.3 wcstol_l.3 wcstoumax_l.3
MLINKS+=wcstombs.3 wcstombs_l.3
MLINKS+=wctomb.3 wctomb_l.3
MLINKS+=wctrans.3 towctrans.3
MLINKS+=wctrans.3 wctrans_l.3 wctrans.3 towctrans_l.3
MLINKS+=wctype.3 iswctype.3
MLINKS+=wctype.3 wctype_l.3 wctype.3 iswctype_l.3
MLINKS+=wcwidth.3 wcwidth_l.3
.endif
