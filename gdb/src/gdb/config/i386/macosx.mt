# APPLE LOCAL file Darwin
# Target: IA86 running Mac OS X

MT_CFLAGS = \
	-DTARGET_I386 \
	-I$(srcdir)/macosx

TDEPFILES = \
	amd64-tdep.o \
	core-macho.o \
	i386-tdep.o \
	i387-tdep.o \
	i386-macosx-tdep.o \
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

DEPRECATED_TM_FILE= tm-i386-macosx.h
