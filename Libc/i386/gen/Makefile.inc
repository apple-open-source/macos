.PATH: ${.CURDIR}/i386/gen

MDSRCS+= _ctx_start.S \
	_setcontext.S \
	getcontext.S \
	getmcontext.c \
	icacheinval.s \
	makecontext.c \
	mcount.s \
	setcontext.c \
	setjmperr.c \
	swapcontext.c \
	cpu_number.s

.for _src in makecontext.c setcontext.c swapcontext.c
CFLAGS-${_src} += -fomit-frame-pointer
# -pg and -fomit-frame-pointer don't work together, so just use -g
${_src:R}.po: ${_src} _STANDARD_DEBUG
.endfor

CFLAGS += -I${.CURDIR}/i386/gen

DYLDSRCS += icacheinval.s \
	cpu_number.s
