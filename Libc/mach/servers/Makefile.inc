.PATH: ${.CURDIR}/${MACHINE_ARCH}/mach/servers ${.CURDIR}/mach/servers

SRVMIGDEFS += srvbootstrap.defs netname.defs

SRVMIGHDRS = ${SRVMIGDEFS:S/.defs$/.h/}
#SRVMIGHDRS = ${SRVMIGDEFS:S/.defs$/.h/:S/^/${.CURDIR}\/mach\/servers\//}
SRVMIGSRCS = ${SRVMIGDEFS:S/.defs$/User.c/}

SRVHDRS = bootstrap_defs.h netname_defs.h key_defs.h nm_defs.h ls_defs.h 
SRVHDRS := ${SRVHDRS:S/^/${.CURDIR}\/mach\/servers\//}
SRVHDRS += ${SRVMIGHDRS}

SRCS+= ${SRVMIGDEFS:S/.defs$/User.defs/}

CLEANFILES += ${SRVMIGHDRS} ${SRVMIGHDRS:S/.h$/User.c/} \
		${SRVMIGHDRS:S/.h$/Server.c/}

${SRVMIGDEFS:S/.defs$/User.So/}:
	mig -user ${.TARGET:S/.So$/.c/} \
		-server ${.TARGET:S/User.So$/Server.c/} \
		-header ${.TARGET:S/User.So$/.h/} \
		${.CURDIR}/mach/servers/${.TARGET:S/User.So$/.defs/}
	${CC} ${CFLAGS} -Os -c ${.TARGET:S/.So$/.c/}\
		-o ${.TARGET}
${SRVMIGDEFS:S/.defs$/User.do/}:
	mig -user ${.TARGET:S/.do$/.c/} \
		-server ${.TARGET:S/User.do$/Server.c/} \
		-header ${.TARGET:S/User.do$/.h/} \
		${.CURDIR}/mach/servers/${.TARGET:S/User.do$/.defs/}
	${CC} -g -DDEBUG ${CFLAGS} -c ${.TARGET:S/.do$/.c/}\
		-o ${.TARGET}
${SRVMIGDEFS:S/.defs$/User.po/}:
	mig -user ${.TARGET:S/.po$/.c/} \
		-server ${.TARGET:S/User.po$/Server.c/} \
		-header ${.TARGET:S/User.po$/.h/} \
		${.CURDIR}/mach/servers/${.TARGET:S/User.po$/.defs/}
	${CC} -pg -DPROFILE ${CFLAGS} -c ${.TARGET:S/.po$/.c/}\
		-o ${.TARGET}
${SRVMIGDEFS:S/.defs$/User.o/}:
	mig -user ${.TARGET:S/.o$/.c/} \
		-server ${.TARGET:S/User.o$/Server.c/} \
		-header ${.TARGET:S/User.o$/.h/} \
		${.CURDIR}/mach/servers/${.TARGET:S/User.o$/.defs/}
	${CC} -static ${CFLAGS} -c ${.TARGET:S/.o$/.c/}\
		-o ${.TARGET}
