#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/regex/Makefile.inc,v 1.6 2001/03/27 17:26:51 ru Exp $

# regex sources
.PATH: ${.CURDIR}/regex

CFLAGS+=-DPOSIX_MISTAKE

.include "Makefile.fbsd_begin"
FBSDSRCS= regcomp.c regerror.c regexec.c regfree.c
FBSDORIGHDRS= cclass.h cname.h engine.c regex2.h utils.h
.include "Makefile.fbsd_end"

.if ${LIB} == "c"
.include "Makefile.fbsd_begin"
FBSDMAN3= regex.3
FBSDMAN7= re_format.7
.include "Makefile.fbsd_end"

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3
MLINKS+=regexec.3 regfree.3
.endif
