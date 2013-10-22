# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=armv7
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=i386 x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=i386 x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

SYSTEM_PRIVATE = -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

SC_PRIV = -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

BLIB = ../bootplib
ARPTEST_BOOTPLIB_CFILES = $(BLIB)/dynarray.c $(BLIB)/ptrlist.c $(BLIB)/interfaces.c $(BLIB)/bpflib.c $(BLIB)/ioregpath.c

arptest: arp_session.o arp_session.h timer.c timer.h FDSet.c FDSet.h
	cc -g -arch i386 -Wall -DTEST_ARP_SESSION $(SYSTEM_PRIVATE) -I../bootplib -o arptest arp_session.o timer.c FDSet.c $(ARPTEST_BOOTPLIB_CFILES) -framework CoreFoundation -framework IOKit

arp_session.o: arp_session.c arp_session.h
	cc -g -arch i386 -Wall -c arp_session.c -DTEST_ARP_SESSION $(SYSTEM_PRIVATE) -I../bootplib

inet6_addrs: ifutil.c rtutil.c ../bootplib/arp.c ../bootplib/IPConfigurationLog.c ../bootplib/util.c
	cc -DTEST_INET6_ADDRLIST -DCONFIGURE_IPV6 -I../bootplib $(SYSTEM_PRIVATE) -framework SystemConfiguration -framework CoreFoundation -g -o $@ $^

dhcpv6: DHCPv6Client.c ../bootplib/DHCPv6.c ../bootplib/DHCPv6Options.c ../bootplib/DHCPDUID.c DHCPv6Socket.c DHCPDUIDIAID.c
	cc -DTEST_DHCPV6_CLIENT -g -Wall DHCPv6Client.c DHCPDUIDIAID.c -o dhcpv6 ../bootplib/DHCPv6Options.c mylog.c timer.c ifutil.c rtutil.c ../bootplib/dhcp_options.c ../bootplib/DHCPv6.c ../bootplib/DHCPDUID.c DHCPv6Socket.c FDSet.c -I../bootplib ../bootplib/ptrlist.c ../bootplib/util.c ../bootplib/DNSNameList.c ../bootplib/interfaces.c ../bootplib/dynarray.c ../bootplib/cfutil.c ../bootplib/arp.c -framework CoreFoundation -framework SystemConfiguration $(SYSTEM_PRIVATE)

dhcpduid: DHCPDUIDIAID.c ../bootplib/util.c ../bootplib/DHCPDUID.c ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/cfutil.c HostUUID.c ../bootplib/IPConfigurationLog.c
	cc -DTEST_DHCPDUIDIAID -I$(BLIB) -g -Wall -framework CoreFoundation -framework SystemConfiguration -o $@ $^

rtadv: RTADVSocket.c ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c timer.c FDSet.c ../bootplib/IPConfigurationLog.c
	cc -D__APPLE_USE_RFC_3542 $(SYSTEM_PRIVATE) -I../bootplib -DTEST_RTADVSOCKET -framework CoreFoundation -framework SystemConfiguration -o $@ $^

rsakey: RSAKey.c ../bootplib/cfutil.c ../bootplib/IPConfigurationLog.c
	cc -I../bootplib -DTEST_RSAKEY -framework SystemConfiguration -framework CoreFoundation -g -Wall -o $@ $^

cga: CGA.c ../bootplib/cfutil.c ../bootplib/IPConfigurationLog.c RSAKey.c HostUUID.c ../bootplib/util.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSTEM_PRIVATE) -I../bootplib -DTEST_CGA -framework SystemConfiguration -framework CoreFoundation $(SC_PRIV) -g -Wall -o $@ $^

ipv6ll: ifutil.c ../bootplib/cfutil.c ../bootplib/util.c ../bootplib/IPConfigurationLog.c rtutil.c ../bootplib/arp.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSTEM_PRIVATE) -I../bootplib -DTEST_IPV6_LL -framework SystemConfiguration -framework CoreFoundation $(SC_PRIV) -g -Wall -o $@ $^

clean:
	rm -f arptest arp_session.o inet6_addrs dhcpv6 dhcpduid cga rsakey ipv6ll
	rm -rf *.dSYM/
