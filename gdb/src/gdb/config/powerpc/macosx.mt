# APPLE LOCAL file Darwin
# Target: PowerPC running Mac OS X

MT_CFLAGS = \
	-DTARGET_POWERPC \
	-I$(srcdir)/macosx

TDEPFILES = \
	core-macho.o \
	xcoffread.o \
	rs6000-tdep.o \
	ppc-sysv-tdep.o \
	ppc-macosx-tdep.o \
	ppc-macosx-frameinfo.o \
	ppc-macosx-regs.o \
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
    macosx-nat-utils.o \
	symread.o \
	pefread.o

DEPRECATED_TM_FILE = tm-ppc-macosx.h
