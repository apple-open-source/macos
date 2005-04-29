.PATH: ${.CURDIR}/emulated

MISRCS+=bsd_signal.c \
	statvfs.c \
	tcgetsid.c

.if ${LIB} == "c"
MAN3+=	bsd_signal.3 statvfs.3 tcgetsid.3

MLINKS+=statvfs.3 fstatvfs.3

.endif
