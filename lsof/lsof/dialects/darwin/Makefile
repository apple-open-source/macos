
# Darwin Makefile
#
# $Id: Makefile,v 1.3 2001/11/01 20:26:13 abe Exp $

PROG=	lsof

BIN=	${DSTROOT}/usr/sbin

DOC=	${DSTROOT}/usr/share/man/man8

I=/usr/include
S=/usr/include/sys
L=/usr/include/local
P=

CDEF=	${RC_CFLAGS}
CDEFS=	${CDEF} ${CFGF}
INCL=	${DINC}
override CFLAGS= ${CDEFS} ${INCL} ${DEBUG}

GRP= kmem

HDR=    lsof.h lsof_fields.h dlsof.h machine.h proto.h dproto.h

SRC=    ddev.c dmnt.c dnode.c dnode1.c dproc.c dsock.c dstore.c \
	arg.c main.c misc.c node.c print.c proc.c store.c usage.c

OBJ=	ddev.o dmnt.o dnode.o dnode1.o dproc.o dsock.o dstore.o \
	arg.o main.o misc.o node.o print.o proc.o store.o usage.o

MAN=	lsof.8

OTHER=	

SHELL=	/bin/sh

SOURCE=	Makefile ${OTHER} ${MAN} ${HDR} ${SRC}

all: ${PROG}

${PROG}: ${LIB} ${P} ${OBJ}
	${CC} -o $@ ${CFLAGS} ${OBJ} ${CFGL}

clean: FRC
	rm -f Makefile.bak ${PROG} a.out core errs lint.out tags *.o version.h
	rm -f machine.h.old new_machine.h
	(cd lib; ${MAKE} -f Makefile.skel clean)

install-strip:
	${MAKE} STRIPFLAGS=-s install

install: all FRC
	mkdir -p ${BIN}
	install -c ${STRIPFLAGS} -m 2755 -g ${GRP} ${PROG} ${BIN}/${PROG}
	mkdir -p ${DOC}
	install -c -m 444 ${MAN} ${DOC}/${MAN}

${LIB}: FRC
	(cd lib; ${MAKE} DEBUG="${DEBUG}" CFGF="${CFGF}")

version.h:	FRC
	@echo Constructing version.h
	@rm -f version.h
	@echo '#define	LSOF_BLDCMT	"${LSOF_BLDCMT}"' > version.h;
	@echo '#define	LSOF_CC		"${CC}"' >> version.h
	@echo '#define	LSOF_CCV	"${CCV}"' >> version.h
	@echo '#define	LSOF_CCDATE	"'`date`'"' >> version.h
	@echo '#define	LSOF_CCFLAGS	"'`echo ${CFLAGS} | sed 's/\\\\(/\\(/g' | sed 's/\\\\)/\\)/g' | sed 's/"/\\\\"/g'`'"' >> version.h
	@if [ "X${LSOF_HOST}" = "X" ]; then \
	  echo '#define	LSOF_HOST	"'`uname -n`'"' >> version.h; \
	else \
	  if [ "${LSOF_HOST}" = "none" ]; then \
	    echo '#define	LSOF_HOST	""' >> version.h; \
	  else \
	    echo '#define	LSOF_HOST	"${LSOF_HOST}"' >> version.h; \
	  fi \
	fi
	@echo '#define	LSOF_LDFLAGS	"${CFGL}"' >> version.h
	@if [ "X${LSOF_LOGNAME}" = "X" ]; then \
	  echo '#define	LSOF_LOGNAME	"${LOGNAME}"' >> version.h; \
	else \
	  if [ "${LSOF_LOGNAME}" = "none" ]; then \
	    echo '#define	LSOF_LOGNAME	""' >> version.h; \
	  else \
	    echo '#define	LSOF_LOGNAME	"${LSOF_LOGNAME}"' >> version.h; \
	  fi; \
	fi
	@if [ "X${LSOF_SYSINFO}" = "X" ]; then \
	    echo '#define	LSOF_SYSINFO	"'`uname -a`'"' >> version.h; \
	else \
	  if [ "${LSOF_SYSINFO}" = "none" ]; then \
	    echo '#define	LSOF_SYSINFO	""' >> version.h; \
	  else \
	    echo '#define	LSOF_SYSINFO	"${LSOF_SYSINFO}"' >> version.h; \
	  fi \
	fi
	@if [ "X${LSOF_USER}" = "X" ]; then \
	  echo '#define	LSOF_USER	"${USER}"' >> version.h; \
	else \
	  if [ "${LSOF_USER}" = "none" ]; then \
	    echo '#define	LSOF_USER	""' >> version.h; \
	  else \
	    echo '#define	LSOF_USER	"${LSOF_USER}"' >> version.h; \
	  fi \
	fi
	@sed '/VN/s/.ds VN \(.*\)/#define	LSOF_VERSION	"\1"/' < version >> version.h

FRC:

# DO NOT DELETE THIS LINE - make depend DEPENDS ON IT

ddev.o:		${HDR} ddev.c

dmnt.o:		${HDR} dmnt.c

dnode.o:	${HDR} dnode.c

dnode1.o:	${HDR} dnode1.c

dproc.o:	${HDR} dproc.c

dsock.o:	${HDR} dsock.c

dstore.o:	${HDR} dstore.c

arg.o:		${HDR} arg.c

main.o:		${HDR} main.c

misc.o:		${HDR} misc.c

node.o:		${HDR} node.c

print.o:	${HDR} print.c

proc.o:		${HDR} proc.c

store.o:	${HDR} store.c

usage.o:	${HDR} version.h usage.c

# *** Do not add anything here - It will go away. ***
