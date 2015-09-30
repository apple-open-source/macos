ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
ARCHS=armv7
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=i386 x86_64
endif

SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

none:
	@echo "make (AFPUsers|bootpdfile|bootplookup|bsdpd)"

AFPUsers: AFPUsers.c AFPUsers.h
	$(CC) -Wall -g $(ARCHS) -DTEST_AFPUSERS $(PF_INC) -I../bootplib -o AFPUsers AFPUsers.c ../bootplib/cfutil.c -framework CoreFoundation -framework OpenDirectory -framework SystemConfiguration

bootpdfile: bootpdfile.c
	cc -Wall -g -arch i386 -arch ppc -DMAIN	-I../bootplib -o bootpdfile bootpdfile.c ../bootplib/hostlist.c

bootplookup: bootplookup.c bootplookup.h 
	cc -Wall -g -arch i386 -arch ppc -DTEST_BOOTPLOOKUP -I../bootplib -o bootplookup bootplookup.c ../bootplib/util.c

bsdpd: bsdpd.c bsdpd.h 
	cc -Wall -g -DTEST_BSDPD -F/System/Library/PrivateFrameworks -I/System/Library/Frameworks/System.framework/PrivateHeaders -I../bootplib -o bsdpd bsdpd.c ../bootplib/subnets.c ../bootplib/cfutil.c ../bootplib/ptrlist.c ../bootplib/util.c ../bootplib/netinfo.c ../bootplib/interfaces.c ../bootplib/bsdplib.c ../bootplib/dhcp_options.c ../bootplib/bootp_transmit.c ../bootplib/NICache.c ../bootplib/nbimages.c ../bootplib/nbsp.c ../bootplib/DNSNameList.c ../bootplib/dynarray.c ../bootplib/in_cksum.c ../bootplib/macnc_options.c ../bootplib/dhcplib.c ../bootplib/inetroute.c ../bootplib/bpflib.c ../bootplib/hostlist.c ../bootplib/host_identifier.c bootplookup.c -framework CoreFoundation -framework OpenDirectory -framework SystemConfiguration -lresolv

type_to_data: type_to_data.c
	$(CC) -g -Wall -o type_to_data type_to_data.c -framework CoreFoundation -lobjc

clean:
	rm -f AFPUsers bootpdfile bootplookup bsdpd type_to_data
	rm -rf *.dSYM/
