# Target: PowerPC running Mac OS X

MT_CFLAGS = \
	-DTARGET_POWERPC \
	-I$(srcdir)/macosx

TDEPFILES = \
	core-macho.o \
	xcoffread.o \
	ppc-macosx-tdep.o \
	ppc-macosx-frameinfo.o \
	ppc-macosx-frameops.o \
	ppc-macosx-regs.o \
	remote-kdp.o \
	kdp-udp.o \
	kdp-transactions.o \
	kdp-protocol.o \
	macosx-tdep.o

TM_FILE = tm-ppc-macosx.h
