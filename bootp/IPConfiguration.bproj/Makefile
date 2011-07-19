BLIB = ../bootplib
ARPTEST_BOOTPLIB_CFILES = $(BLIB)/dynarray.c $(BLIB)/ptrlist.c $(BLIB)/interfaces.c $(BLIB)/bpflib.c $(BLIB)/ioregpath.c

arptest: arp_session.o arp_session.h timer.c timer.h FDSet.c FDSet.h
	cc -g -arch i386 -Wall -DTEST_ARP_SESSION -I/System/Library/Frameworks/System.framework/PrivateHeaders -I../bootplib -o arptest arp_session.o timer.c	FDSet.c $(ARPTEST_BOOTPLIB_CFILES) -framework CoreFoundation -framework IOKit

arp_session.o: arp_session.c arp_session.h
	cc -g -arch i386 -Wall -c arp_session.c -DTEST_ARP_SESSION -I/System/Library/Frameworks/System.framework/PrivateHeaders -I../bootplib

inet6_addrs: ifutil.c ifutil.h
	cc -g -Wall ifutil.c rtutil.c mylog.c ../bootplib/arp.c -DTEST_INET6_ADDRLIST -DCONFIGURE_IPV6 -o inet6_addrs -I../bootplib -I/System/Library/Frameworks/System.framework/PrivateHeaders

dhcpv6: DHCPv6Client.c ../bootplib/DHCPv6.c ../bootplib/DHCPv6Options.c ../bootplib/DHCPDUID.c DHCPv6Socket.c DHCPDUIDIAID.c
	cc -DTEST_DHCPV6_CLIENT -g -Wall DHCPv6Client.c DHCPDUIDIAID.c -o dhcpv6 ../bootplib/DHCPv6Options.c mylog.c timer.c ifutil.c rtutil.c ../bootplib/dhcp_options.c ../bootplib/DHCPv6.c ../bootplib/DHCPDUID.c DHCPv6Socket.c FDSet.c -I../bootplib ../bootplib/ptrlist.c ../bootplib/util.c ../bootplib/DNSNameList.c ../bootplib/interfaces.c ../bootplib/dynarray.c ../bootplib/cfutil.c ../bootplib/arp.c -framework CoreFoundation -framework SystemConfiguration -I/System/Library/Frameworks/System.framework/PrivateHeaders

dhcpduid: DHCPDUIDIAID.c
	cc -O0 -DTEST_DHCPDUIDIAID DHCPDUIDIAID.c -g -Wall -o dhcpduid mylog.c ../bootplib/util.c ../bootplib/DHCPDUID.c -I../bootplib ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/cfutil.c -framework CoreFoundation -framework SystemConfiguration

rtadv: RTADVSocket.c RTADVSocket.h
	cc -I../bootplib -DTEST_RTADVSOCKET RTADVSocket.c -g -Wall -o rtadv ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c timer.c FDSet.c mylog.c  -framework CoreFoundation -I/System/Library/Frameworks/System.framework/PrivateHeaders

rtadv.i: RTADVSocket.c RTADVSocket.h
	cc -E -I../bootplib -DTEST_RTADVSOCKET RTADVSocket.c -g -Wall ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c timer.c FDSet.c mylog.c  -framework CoreFoundation -I/System/Library/Frameworks/System.framework/PrivateHeaders > rtadv.i

clean:
	rm -f arptest arp_session.o inet6_addrs dhcpv6 dhcpduid
	rm -rf *.dSYM/
