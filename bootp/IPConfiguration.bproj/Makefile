# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=arm64
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc -fno-color-diagnostics
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

SYSTEM_PRIVATE = -I$(SYSROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

SC_PRIV = -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS

BLIB = ../bootplib
IBLIB = -I$(BLIB)

ARPTEST_BOOTPLIB_CFILES = $(BLIB)/dynarray.c $(BLIB)/ptrlist.c $(BLIB)/interfaces.c $(BLIB)/bpflib.c $(BLIB)/ioregpath.c $(BLIB)/IPConfigurationLog.c $(BLIB)/util.c $(BLIB)/cfutil.c

IFUTIL_FILES = ifutil.c CGA.c ../bootplib/util.c ../bootplib/cfutil.c ../bootplib/IPConfigurationLog.c HostUUID.c

arptest: FDSet.c IPConfigurationAgentUtil.c arp_session.c timer.c $(ARPTEST_BOOTPLIB_CFILES)
	$(CC)  -DSC_LOG_HANDLE=IPConfigLogGetHandle -isysroot $(SYSROOT) $(ARCH_FLAGS) -DTEST_ARP_SESSION $(PF_INC) -g -Wall $(IBLIB) -framework CoreFoundation -framework SystemConfiguration -framework Apple80211 -framework IOKit $(SYSTEM_PRIVATE) -o $@ $^

test_timer: timer.c IPConfigurationAgentUtil.c  $(ARPTEST_BOOTPLIB_CFILES)
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -DTEST_TIMER $(PF_INC) -g -Wall $(IBLIB) -framework CoreFoundation -framework SystemConfiguration -framework Apple80211 -framework IOKit $(SYSTEM_PRIVATE) -o $@ $^

inet6_addrs: $(IFUTIL_FILES) rtutil.c ../bootplib/arp.c
	cc -DTEST_INET6_ADDRLIST -DCONFIGURE_IPV6 $(IBLIB) $(SYSTEM_PRIVATE) -framework SystemConfiguration -framework CoreFoundation -g -o $@ $^

dhcpv6: IPConfigurationAgentUtil.c DHCPv6Client.c ifutil.c CGA.c FDSet.c timer.c HostUUID.c ../bootplib/IPv6Socket.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/IPConfigurationLog.c ../bootplib/interfaces.c ../bootplib/DHCPv6.c ../bootplib/DHCPv6Options.c ../bootplib/DHCPDUID.c DHCPv6Socket.c DHCPDUIDIAID.c ../bootplib/DNSNameList.c ../bootplib/cfutil.c ../bootplib/util.c wireless.c
	$(CC) -DMYLOG_STDOUT -isysroot $(SYSROOT) $(ARCH_FLAGS) -DSC_LOG_HANDLE=IPConfigLogGetHandle -D__APPLE_USE_RFC_3542 -DTEST_DHCPV6_CLIENT $(PF_INC) -g -Wall $(IBLIB) -framework CoreFoundation -framework SystemConfiguration -framework Apple80211 -framework IOKit $(SYSTEM_PRIVATE) -o $@ $^

dhcpduid: DHCPDUIDIAID.c ../bootplib/util.c ../bootplib/DHCPDUID.c ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/cfutil.c HostUUID.c ../bootplib/IPConfigurationLog.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -DTEST_DHCPDUIDIAID $(IBLIB) -g -Wall -framework CoreFoundation -framework SystemConfiguration $(SYSTEM_PRIVATE) -o $@ $^

rtadv: IPConfigurationAgentUtil.c RTADVSocket.c ICMPv6Socket.c ../bootplib/cfutil.c ../bootplib/IPv6Socket.c ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c timer.c FDSet.c ../bootplib/IPConfigurationLog.c ../bootplib/util.c ../bootplib/DNSNameList.c ../bootplib/RouterAdvertisement.c
	$(CC) -DMYLOG_STDOUT -isysroot $(SYSROOT) $(ARCH_FLAGS) -D__APPLE_USE_RFC_3542 $(SYSTEM_PRIVATE) $(IBLIB) -DTEST_RTADVSOCKET $(PF_INC) -framework CoreFoundation -framework SystemConfiguration -framework Apple80211 -framework IOKit $(SYSTEM_PRIVATE) -g -o $@ $^

cga: CGA.c ../bootplib/cfutil.c ../bootplib/IPConfigurationLog.c HostUUID.c ../bootplib/util.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSTEM_PRIVATE) $(IBLIB) -DTEST_CGA -framework SystemConfiguration -framework CoreFoundation $(SC_PRIV) -g -Wall -o $@ $^

ndadvert: IPv6Socket.c ../bootplib/interfaces.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/IPConfigurationLog.c ifutil.c CGA.c ../bootplib/util.c ../bootplib/cfutil.c HostUUID.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -D__APPLE_USE_RFC_3542 $(SYSTEM_PRIVATE) $(IBLIB) -DTEST_NEIGHBOR_ADVERT -framework SystemConfiguration -framework CoreFoundation $(SC_PRIV) -g -Wall -o $@ $^

ipv6ll: $(IFUTIL_FILES) rtutil.c ../bootplib/arp.c
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) $(SYSTEM_PRIVATE) $(IBLIB) -DTEST_IPV6_LL -framework SystemConfiguration -framework CoreFoundation $(SC_PRIV) -g -Wall -o $@ $^

IPv6AWDReportTest: IPv6AWDReport.m
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -g  -Wall -DTEST_IPV6_AWD_REPORT -framework Foundation -weak_framework WirelessDiagnostics -framework CoreFoundation -framework SystemConfiguration -framework ProtocolBuffer $(SC_PRIV) $(PF_INC) -I AWD $(IBLIB) ../bootplib/IPConfigurationLog.c AWD/AWDIPConfigurationIPv6Report.m -o $@ $^

ipv6lladdr: $(IFUTIL_FILES)
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -g  -Wall -DTEST_IPV6_LINKLOCAL_ADDRESS -framework CoreFoundation -framework SystemConfiguration $(SC_PRIV) $(SYSTEM_PRIVATE) $(IBLIB) -o $@ $^

ipv6defpfx: $(IFUTIL_FILES)
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -g  -Wall -DTEST_IPV6_ROUTER_PREFIX_COUNT -framework CoreFoundation -framework SystemConfiguration $(SC_PRIV) $(SYSTEM_PRIVATE) $(IBLIB) -o $@ $^

protolist: $(IFUTIL_FILES)
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -g  -Wall -DTEST_PROTOLIST -framework CoreFoundation -framework SystemConfiguration $(SC_PRIV) $(SYSTEM_PRIVATE) $(IBLIB) -o $@ $^

verbose_sysctls: $(IFUTIL_FILES)
	$(CC) -isysroot $(SYSROOT) $(ARCH_FLAGS) -g  -Wall -DMYLOG_STDOUT -DTEST_VERBOSE_SYSCTLS -framework CoreFoundation -framework SystemConfiguration $(SC_PRIV) $(SYSTEM_PRIVATE) $(IBLIB) -o $@ $^

wireless: wireless.c
	$(CC) -DTEST_WIRELESS -isysroot $(SYSROOT) $(PF_INC) $(ARCH_FLAGS) -g  -Wall -framework CoreFoundation -framework SystemConfiguration $(SC_PRIV) $(SYSTEM_PRIVATE) $(IBLIB) ../bootplib/util.c ../bootplib/cfutil.c ../bootplib/IPConfigurationLog.c -framework IO80211 -framework IOKit -o $@ $^
	codesign -s - -f --entitlements wireless-entitlements.plist $@

bootp_session: bootp_session.c IPConfigurationAgentUtil.c FDSet.c timer.c ../bootplib/dhcp_options.c ../bootplib/ptrlist.c ../bootplib/dynarray.c ../bootplib/IPConfigurationLog.c ../bootplib/dhcplib.c ../bootplib/in_cksum.c ../bootplib/bpflib.c ../bootplib/DNSNameList.c ../bootplib/udp_transmit.c ../bootplib/IPv4ClasslessRoute.c ../bootplib/interfaces.c ../bootplib/cfutil.c ../bootplib/util.c
	$(CC) -DMYLOG_STDOUT -isysroot $(SYSROOT) $(ARCH_FLAGS) -D__APPLE_USE_RFC_3542 -DTEST_BOOTP_SESSION $(PF_INC) -g -Wall $(IBLIB) -framework CoreFoundation -framework SystemConfiguration -framework Apple80211 -framework IOKit $(SYSTEM_PRIVATE) -o $@ $^

clean:
	rm -f *~ arptest arp_session.o inet6_addrs dhcpv6 dhcpduid cga ipv6ll ipv6defpfx ipv6lladdr IPv6AWDReportTest rtadv wireless bootp_session test_timer
	rm -rf *.dSYM/
