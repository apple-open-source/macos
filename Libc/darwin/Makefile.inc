.PATH: ${.CURDIR}/darwin

.ifnmake autopatch
.if exists(${.CURDIR}/${MACHINE_ARCH}/darwin/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/darwin/Makefile.inc"
.endif
.endif # !autopatch

DARWINMIGDEFS += dirhelper.defs
DARWINMIGHDRS += ${DARWINMIGDEFS:.defs=.h}
DARWINMIGSRCS += ${DARWINMIGDEFS:.defs=User.c}

INSTHDRS += ${.CURDIR}/darwin/libproc.h

LOCALHDRS += ${.CURDIR}/darwin/dirhelper.defs \
	${.CURDIR}/darwin/dirhelper_priv.h \
	${.CURDIR}/darwin/libproc.h
MISRCS += ${DARWINMIGSRCS} _dirhelper.c libproc.c proc_listpidspath.c

.ifndef LP64
MISRCS += kvm.c MKGetTimeBaseInfo.c
.endif

# force building of the mig stuff before _dirhelper.c
_dirhelper.So _dirhelper.po _dirhelper.do _dirhelper.o: dirhelperUser.c

CFLAGS-_dirhelper.c += -I${OBJROOT}
CFLAGS-proc_listpidspath.c += -I${.CURDIR}/darwin
