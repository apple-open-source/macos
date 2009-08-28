# APPLE LOCAL file Darwin
# Target: ARM running Mac OS X

MT_CFLAGS = \
	-DTARGET_ARM \
	-I$(srcdir)/macosx

TDEPFILES = \
	core-macho.o \
	xcoffread.o \
	arm-tdep.o \
	arm-macosx-tdep.o \
	arm-macosx-nat-exec.o \
	remote-kdp.o \
	kdp-udp.o \
	kdp-transactions.o \
	kdp-protocol.o \
	remote-mobile.o \
        macosx-tdep.o \
        machoread.o \
	macosx-nat-cmds-load.o \
       	macosx-nat-dyld.o \
	macosx-nat-dyld-path.o \
	macosx-nat-dyld-info.o \
	macosx-nat-dyld-process.o \
	macosx-nat-dyld-io.o \
	macosx-nat-utils.o

DEPRECATED_TM_FILE = tm-arm-macosx.h

CFM_CFLAGS = -DWITH_CFM=0
