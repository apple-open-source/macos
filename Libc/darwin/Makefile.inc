.PATH: ${.CURDIR}/darwin

.sinclude "${.CURDIR}/${MACHINE_ARCH}/darwin/Makefile.inc"

DARWINMIGDEFS += dirhelper.defs
DARWINMIGHDRS += ${DARWINMIGDEFS:.defs=.h}
DARWINMIGSRCS += ${DARWINMIGDEFS:.defs=User.c}

INSTHDRS += ${.CURDIR}/darwin/libproc.h

LOCALHDRS += ${.CURDIR}/darwin/dirhelper.defs \
	${.CURDIR}/darwin/dirhelper_priv.h \
	${.CURDIR}/darwin/libproc.h \
	${.CURDIR}/darwin/libproc_internal.h 
MISRCS += ${DARWINMIGSRCS} _dirhelper.c libproc.c proc_listpidspath.c

.ifndef LP64
MISRCS += kvm.c MKGetTimeBaseInfo.c
.endif

# force building of the mig stuff before _dirhelper.c
_dirhelper.${OBJSUFFIX}: dirhelperUser.c

CFLAGS-proc_listpidspath.c += -I${.CURDIR}/darwin
