# searching i386 directory as a fallback to avoid unnecessary code duplication
.PATH: ${.CURDIR}/x86_64/gen ${.CURDIR}/i386/gen

MDSRCS+= _ctx_start.S \
	_setcontext.S \
	getcontext.S \
	getmcontext.c \
	icacheinval.s \
	makecontext.c \
	mcount.s \
	setcontext.c \
	setjmperr.c \
	swapcontext.c

.for _src in makecontext.c setcontext.c swapcontext.c
CFLAGS-${_src} += -fomit-frame-pointer
# -pg and -fomit-frame-pointer don't work together, so just use -g
${_src:R}.po: ${_src} _STANDARD_DEBUG
.endfor
