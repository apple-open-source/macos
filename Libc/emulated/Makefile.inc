.PATH: ${.CURDIR}/emulated

MISRCS+=bsd_signal.c \
	lchflags.c \
	lchmod.c \
	lutimes.c \
	statvfs.c \
	tcgetsid.c

.if ${LIB} == "c"
MAN3+=	bsd_signal.3 lchflags.3 lchmod.3 lutimes.3 statvfs.3 tcgetsid.3

MLINKS+=statvfs.3 fstatvfs.3

.endif
