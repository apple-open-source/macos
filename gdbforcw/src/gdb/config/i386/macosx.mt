# Target: IA86 running Mac OS X

MT_CFLAGS = \
	-DTARGET_I386 \
	-I$(srcdir)/macosx

TDEPFILES = \
	core-macho.o \
	i386-tdep.o \
	i387-tdep.o \
	i386-macosx-tdep.o \
	remote-kdp.o \
	kdp-udp.o \
	kdp-transactions.o \
	kdp-protocol.o \
	macosx-tdep.o \
	machoread.o \
	symread.o \
	pefread.o

TM_FILE= tm-i386-macosx.h
