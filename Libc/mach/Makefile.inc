.PATH: ${.CURDIR}/${MACHINE_ARCH}/mach ${.CURDIR}/mach

.include "${.CURDIR}/mach/headers/Makefile.inc"
.include "${.CURDIR}/mach/servers/Makefile.inc"

# machine-dependent mach sources  
.if exists(${.CURDIR}/${MACHINE_ARCH}/mach/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/mach/Makefile.inc"
.endif

MIGDEFS += \
	clock.defs \
	clock_priv.defs \
	host_priv.defs \
	host_security.defs \
	ledger.defs \
	lock_set.defs \
	mach_port.defs \
	mach_host.defs \
	processor.defs \
	processor_set.defs \
	task.defs \
	thread_act.defs \
	vm_map.defs

#MIGHDRS = ${MIGDEFS:S/.defs$/.h/:S/^/${.CURDIR}\/mach\//}
MIGHDRS = ${MIGDEFS:S/.defs$/.h/}
MIGHDRS += clock_reply.h
MACH_INSTHDRS += ${MIGHDRS}

# These files are generated from the .defs files
MIGSRCS = ${MIGDEFS:S/.defs$/User.c/}

SRCS+=  ${MIGDEFS:S/.defs$/User.defs/} \
	bootstrap_ports.c \
	clock_sleep.c \
	error_codes.c \
	excServer.c \
	excUser.c \
	exc_catcher.c \
	exc_catcher_state.c \
	exc_catcher_state_identity.c \
	fprintf_stderr.c \
	mig_allocate.c \
	mig_deallocate.c \
	mig_reply_setup.c  \
	mig_strncpy.c \
	mach_error.c \
	mach_error_string.c \
	mach_init.c \
	mach_init_ports.c \
	mach_msg.c \
	mach_traps.s \
	ms_thread_switch.c  \
	notifyUser.c \
	panic.c  \
	port_obj.c  \
	sbrk.c  \
	semaphore.c  \
	slot_name.c 

CLEANFILES += ${MIGHDRS} ${MIGHDRS:S/.h$/User.c/} \
		${MIGHDRS:S/.h/Server.c/} exc.h excUser.c excServer.c \
		notify.h notifyUser.c notifyServer.c

${MIGDEFS:S/.defs$/User.So/}:
	mig -user ${.TARGET:S/.So$/.c/} \
		-server ${.TARGET:S/User.So$/Server.c/} \
		-header ${.TARGET:S/User.So$/.h/} \
		${.CURDIR}/mach/${.TARGET:S/User.So$/.defs/}
	${CC} ${CFLAGS} -Os -c ${.TARGET:S/.So$/.c/}\
		-o ${.TARGET}
${MIGDEFS:S/.defs$/User.do/}:
	mig -user ${.TARGET:S/.do$/.c/} \
		-server ${.TARGET:S/User.do$/Server.c/} \
		-header ${.TARGET:S/User.do$/.h/} \
		${.CURDIR}/mach/${.TARGET:S/User.do$/.defs/}
	${CC} -g -DDEBUG ${CFLAGS} -c ${.TARGET:S/.do$/.c/}\
		-o ${.TARGET}
${MIGDEFS:S/.defs$/User.po/}:
	mig -user ${.TARGET:S/.po$/.c/} \
		-server ${.TARGET:S/User.po$/Server.c/} \
		-header ${.TARGET:S/User.po$/.h/} \
		${.CURDIR}/mach/${.TARGET:S/User.po$/.defs/}
	${CC} -pg -DPROFILE ${CFLAGS} -c ${.TARGET:S/.po$/.c/}\
		-o ${.TARGET}
${MIGDEFS:S/.defs$/User.o/}:
	mig -user ${.TARGET:S/.o$/.c/} \
		-server ${.TARGET:S/User.o$/Server.c/} \
		-header ${.TARGET:S/User.o$/.h/} \
		${.CURDIR}/mach/${.TARGET:S/User.o$/.defs/}
	${CC} -static ${CFLAGS} -c ${.TARGET:S/.o$/.c/}\
		-o ${.TARGET}

excServer.c: ${.CURDIR}/mach/exc.defs
	mig -user excUser.c -server excServer.c -header exc.h \
		${.CURDIR}/mach/exc.defs
excUser.c: ${.CURDIR}/mach/exc.defs
	mig -user excUser.c -server excServer.c -header exc.h \
		${.CURDIR}/mach/exc.defs
notifyUser.c: ${.CURDIR}/mach/notify.defs
	mig -user notifyUser.c -server notifyServer.c -header notify.h \
		${.CURDIR}/mach/notify.defs
