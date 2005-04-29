.PATH: ${.CURDIR}/darwin

MISRCS += copyfile.c
LOCALHDRS += ${.CURDIR}/darwin/copyfile.h
CFLAGS-copyfile.c = -I${.CURDIR}/darwin
