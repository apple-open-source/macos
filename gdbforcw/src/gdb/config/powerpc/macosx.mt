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
	macosx-tdep.o \
	machoread.o \
	symread.o \
	pefread.o

TM_FILE = tm-ppc-macosx.h
