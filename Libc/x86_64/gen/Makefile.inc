# searching i386 directory as a fallback to avoid unnecessary code duplication
.PATH: ${.CURDIR}/x86_64/gen ${.CURDIR}/i386/gen

MDSRCS+= icacheinval.s \
	mcount.s \
	setjmperr.c
