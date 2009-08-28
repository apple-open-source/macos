none:
	@echo "make what?"

gen: genoptionfiles
	rm -f gen_dhcp*.h
	cc -Wall -g -o genoptionfiles genoptionfiles.c
	./genoptionfiles -table > gen_dhcp_parse_table.h
	./genoptionfiles -dhcptag > gen_dhcp_tags.h
	./genoptionfiles -dhcptype > gen_dhcp_types.h
	chmod 444 gen_dhcp*.h

inetroute: inetroute.c
	cc -Wall -g -o inetroute -DTEST_INETROUTE inetroute.c dynarray.c ptrlist.c util.c

sharepoints:
	cc -Wno-four-char-constants -o sharepoints $(OTHER_CFLAGS) -DTEST_SHAREPOINTS sharepoints.c -F$(NEXT_ROOT)/System/Library/PrivateFrameworks -framework ServerControl -framework ServerPrefix -framework AFPDefines

nilist: NICache.c NICache.h
	cc -Wall -g -o nilist -DNICACHE_TEST NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c NIDomain.c

afpusers:
	cc -Wall -g -o afpusers -DTEST_AFPUSERS AFPUsers.c NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c NIDomain.c

readtest:
	cc -Wall -g -o readtest -DREAD_TEST NICache.c dynarray.c ptrlist.c netinfo.c host_identifier.c util.c

arp: arp.c arp.h
	cc -Wall -g -o arp -DMAIN arp.c

nihosts:
	cc -Wall -g -o nihosts -DTEST_NIHOSTS NIHosts.m util.c NIDomain.c dynarray.c ptrlist.c netinfo.c host_identifier.c

interfaces: interfaces.c interfaces.h
	cc -Wall -g -o interfaces -DTEST_INTERFACES interfaces.c ptrlist.c dynarray.c -I$(NET_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/System.framework/PrivateHeaders

dhcpopt: dhcp_options.c
	cc -Wall -g -DTEST_DHCP_OPTIONS -o dhcpopt dhcp_options.c ptrlist.c util.c DNSNameList.c

macncopt: macnc_options.c
	cc -Wall -g -DTEST_MACNC_OPTIONS -o macncopt macnc_options.c dhcp_options.c ptrlist.c util.c DNSNameList.c

nbsp: nbsp.c nbsp.h
	cc -Wall -g -DTEST_NBSP -o nbsp nbsp.c ptrlist.c dynarray.c

hfsvols: hfsvols.c hfsvols.h
	cc -Wall -g -DTEST_HFSVOLS -o hfsvols hfsvols.c ptrlist.c dynarray.c

nbimages: nbimages.c nbimages.h cfutil.c
	cc -arch i386 -arch ppc -Wall -g -DTEST_NBIMAGES -o nbimages nbimages.c util.c nbsp.c ptrlist.c dynarray.c cfutil.c -framework CoreFoundation -I$(NET_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/System.framework/PrivateHeaders

dnsnamelist: DNSNameList.c DNSNameList.h
	cc -arch i386 -arch ppc -Wall -g -DTEST_DNSNAMELIST -o dnsnamelist DNSNameList.c util.c

subnets: subnets.c cfutil.c DNSNameList.c util.c ptrlist.c dynarray.c dhcp_options.c
	cc -arch i386 -arch ppc -g -Wall -DTEST_SUBNETS -o subnets subnets.c cfutil.c util.c ptrlist.c dynarray.c DNSNameList.c dhcp_options.c -framework CoreFoundation

clean:
	rm -f afpusers arp dhcpopt dnsnamelist genoptionfiles hfsvols interfaces nbimages nbsp nihosts nilist readtest sharepoints subnets
	rm -rf *.dSYM/
