BLIB = ../bootplib
ARPTEST_BOOTPLIB_CFILES = $(BLIB)/dynarray.c $(BLIB)/ptrlist.c $(BLIB)/interfaces.c $(BLIB)/bpflib.c $(BLIB)/ioregpath.c

arptest: arp_session.o arp_session.h timer.c timer.h FDSet.c FDSet.h
	cc -g -arch i386 -Wall -DTEST_ARP_SESSION -I/System/Library/Frameworks/System.framework/PrivateHeaders -I../bootplib -o arptest arp_session.o timer.c	FDSet.c $(ARPTEST_BOOTPLIB_CFILES) -framework CoreFoundation -framework IOKit

arp_session.o: arp_session.c arp_session.h
	cc -g -arch i386 -Wall -c arp_session.c -DTEST_ARP_SESSION -I/System/Library/Frameworks/System.framework/PrivateHeaders -I../bootplib

clean:
	rm -f arptest arp_session.o
