EXTRA_CFLAGS=

all: test_proxy

# ----------

shared_dns_info.h shared_dns_infoUser.c: ../../dnsinfo/shared_dns_info.defs
	mig ../../dnsinfo/shared_dns_info.defs

shared_dns_infoUser.o: shared_dns_info.h shared_dns_infoUser.c
	cc -I../../dnsinfo -Wall -O0 -g -c shared_dns_infoUser.c

dnsinfo_create.o: shared_dns_info.h ../../dnsinfo/dnsinfo_create.h ../../dnsinfo/dnsinfo_create.c
	cc -I. -I../../dnsinfo -I../../nwi -Wall -O0 -g -c ../../dnsinfo/dnsinfo_create.c

dnsinfo_flatfile.o: ../../dnsinfo/dnsinfo_copy.c ../../dnsinfo/dnsinfo_flatfile.c shared_dns_info.h
	cc -I../../dnsinfo -I../../nwi -D_PATH_RESOLVER_DIR='"/var/tmp/resolver"' -Wall -O0 -g -c ../../dnsinfo/dnsinfo_flatfile.c

dnsinfo_private.o: ../../dnsinfo/dnsinfo_private.h ../../dnsinfo/dnsinfo_private.c
	cc -I../../dnsinfo -Wall -O0 -g -c ../../dnsinfo/dnsinfo_private.c

dns-configuration.o: dns-configuration.h dns-configuration.c dnsinfo_create.o
	cc -I. -I../../dnsinfo -I../../nwi -DMAIN -Wall -O0 -g -c dns-configuration.c

network_information_priv.o: ../../nwi/network_information_priv.h ../../nwi/network_information_priv.c
	cc -I. -I../../dnsinfo -I../../nwi -DMAIN -Wall -O0 -g -c ../../nwi/network_information_priv.c

proxy-configuration.o: proxy-configuration.h proxy-configuration.c
	cc -I. -Wall -O0 -g -c proxy-configuration.c

smb-configuration.o: smb-configuration.c
	cc -I. -Wall -O0 -g -c smb-configuration.c

# ----------

test_dns: Makefile dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o shared_dns_infoUser.o dns-configuration.o
	cc -o test_dns dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o shared_dns_infoUser.o dns-configuration.o ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

test_proxy: Makefile proxy-configuration.h proxy-configuration.c
	cc -Wall -O0 -g -o test_proxy -DMAIN -DDEBUG proxy-configuration.c ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

test_smb: Makefile smb-configuration.h smb-configuration.c
	cc -Wall -O0 -g -o test_smb -DMAIN -DDEBUG smb-configuration.c ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

test_ipv4_routelist.o: ip_plugin.c
	cc -I. -I../../dnsinfo -I../../nwi -DTEST_IPV4_ROUTELIST -Wall -O0 -g -c -o test_ipv4_routelist.o ip_plugin.c

test_ipv4_routelist: test_ipv4_routelist.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o shared_dns_infoUser.o network_information_priv.o smb-configuration.o proxy-configuration.o
	cc -Wall -O0 -g -o test_ipv4_routelist test_ipv4_routelist.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o network_information_priv.o shared_dns_infoUser.o smb-configuration.o proxy-configuration.o -framework SystemConfiguration -framework CoreFoundation

test_ipv4_routelist_reference.txt: test_ipv4_routelist
	sh test_reference.sh create test_ipv4_routelist test_ipv4_routelist_reference.txt test_ipv4_routelist_filter.sh

test_ipv4_routelist_test: test_ipv4_routelist
	sh test_reference.sh test test_ipv4_routelist test_ipv4_routelist_reference.txt test_ipv4_routelist_filter.sh

# ----------

IPMonitor.o: ip_plugin.c
	cc -I. -I../../dnsinfo -I../../nwi -DTEST_IPMONITOR -Wall -O0 -g -c -o IPMonitor.o ip_plugin.c

IPMonitor: IPMonitor.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o shared_dns_infoUser.o smb-configuration.o
	cc -Wall -O0 -g -o IPMonitor IPMonitor.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_private.o shared_dns_infoUser.o smb-configuration.o -framework SystemConfiguration -framework CoreFoundation

# ----------

clean:
	rm -rf	*.o								\
		shared_dns_info.h shared_dns_infoServer.c shared_dns_infoUser.c	\
		test_dns test_dns.dSYM						\
		test_proxy test_proxy.dSYM					\
		test_smb test_smb.dSYM						\
		test_ipv4_routelist test_ipv4_routelist.dSYM			\
		IPMonitor IPMonitor.dSYM
