# Target: Intel 386 running Rhapsody

MT_CFLAGS = \
	-DTARGET_I386

TDEPFILES = \
	core-macho.o \
	xcoffread.o \
	i386-tdep.o \
	i387-tdep.o \
	i386-next-tdep.o \
	remote-kdp.o \
	kdp-udp.o \
	kdp-transactions.o \
	kdp-protocol.o \
	nextstep-tdep.o

TM_FILE= tm-i386-next.h
