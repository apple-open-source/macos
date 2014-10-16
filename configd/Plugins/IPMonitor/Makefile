EXTRA_CFLAGS=

TEST_INCLUDE=-I. -I../common -I../../dnsinfo -I../../nwi -I../../libSystemConfiguration -I../../SystemConfiguration.fproj -I../../IPMonitorControl -I/System/Library/Frameworks/System.framework/PrivateHeaders

all: test_ipv4_routelist test_ipv6_routelist

# ----------

dnsinfo_create.o: ../../dnsinfo/dnsinfo_create.h ../../dnsinfo/dnsinfo_create.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../dnsinfo/dnsinfo_create.c

dnsinfo_flatfile.o: ../../dnsinfo/dnsinfo_copy.c ../../dnsinfo/dnsinfo_flatfile.c
	cc ${TEST_INCLUDE} -D_PATH_RESOLVER_DIR='"/var/tmp/resolver"' -Wall -O0 -g -c ../../dnsinfo/dnsinfo_flatfile.c

dnsinfo_server.o: ../../dnsinfo/dnsinfo_copy.c ../../dnsinfo/dnsinfo_server.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../dnsinfo/dnsinfo_server.c

dns-configuration.o: dns-configuration.h dns-configuration.c dnsinfo_create.o
	cc ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c dns-configuration.c

network_information_priv.o: ../../nwi/network_information_priv.h ../../nwi/network_information_priv.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../nwi/network_information_priv.c

network_information_server.o: ../../nwi/network_information_server.h ../../nwi/network_information_server.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../nwi/network_information_server.c

proxy-configuration.o: proxy-configuration.h proxy-configuration.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c proxy-configuration.c

set-hostname.o: set-hostname.c
	cc ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c set-hostname.c

smb-configuration.o: smb-configuration.c
	cc ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c smb-configuration.c

libSystemConfiguration_client.o: ../../libSystemConfiguration/libSystemConfiguration_client.h ../../libSystemConfiguration/libSystemConfiguration_client.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../libSystemConfiguration/libSystemConfiguration_client.c

libSystemConfiguration_server.o: ../../libSystemConfiguration/libSystemConfiguration_server.h ../../libSystemConfiguration/libSystemConfiguration_server.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../../libSystemConfiguration/libSystemConfiguration_server.c

IPMonitorControlPrefs.o: ../common/IPMonitorControlPrefs.h ../common/IPMonitorControlPrefs.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c ../common/IPMonitorControlPrefs.c

IPMonitorControlServer.o: ../../IPMonitorControl/IPMonitorControlServer.c
	cc ${TEST_INCLUDE} -Wall -O0 -g -c $^

# ----------

dns-configurationX.o: Makefile dns-configuration.h dns-configuration.c dnsinfo_create.o
	cc -DMAIN ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o dns-configurationX.o dns-configuration.c

ip_pluginX.o: Makefile ip_plugin.c
	cc -DTEST_DNS ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o ip_pluginX.o ip_plugin.c

test_dns: Makefile dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configurationX.o proxy-configuration.o set-hostname.o smb-configuration.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o
	cc -Wall -O0 -g -o test_dns dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configurationX.o proxy-configuration.o set-hostname.o smb-configuration.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o -framework SystemConfiguration -framework CoreFoundation

# ----------

test_proxy: Makefile proxy-configuration.h proxy-configuration.c
	cc -DMAIN -DDEBUG -Wall -O0 -g -o test_proxy proxy-configuration.c ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

set-hostnameX.o: Makefile set-hostname.h set-hostname.c
	cc -DMAIN -DDEBUG ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o set-hostnameX.o set-hostname.c

test_hostname: Makefile dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configuration.o smb-configuration.o proxy-configuration.o set-hostnameX.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o
	cc -Wall -O0 -g -o test_hostname dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configuration.o smb-configuration.o proxy-configuration.o set-hostnameX.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

smb-configurationX.o: smb-configuration.h smb-configuration.c
	cc -DMAIN -DDEBUG ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o smb-configurationX.o smb-configuration.c

test_smb: Makefile dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configuration.o smb-configurationX.o proxy-configuration.o set-hostname.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o
	cc -Wall -O0 -g -o test_smb dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o ip_pluginX.o IPMonitorControlPrefs.o network_information_priv.o network_information_server.o dns-configuration.o smb-configurationX.o proxy-configuration.o set-hostname.o libSystemConfiguration_client.o libSystemConfiguration_server.o IPMonitorControlServer.o ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

test_ipv4_routelist.o: ip_plugin.c
	cc -DTEST_IPV4_ROUTELIST ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o $@ $^

test_ipv4_routelist: test_ipv4_routelist.o IPMonitorControlPrefs.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o network_information_priv.o network_information_server.o smb-configuration.o proxy-configuration.o libSystemConfiguration_server.o IPMonitorControlServer.o
	cc -Wformat -Wall -O0 -g -o $@ $^ ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

test_ipv4_routelist_reference.txt: test_ipv4_routelist
	sh test_reference.sh create test_ipv4_routelist test_ipv4_routelist_reference.txt routelist_output_filter.sh

test_ipv4_routelist_test: test_ipv4_routelist
	sh test_reference.sh test test_ipv4_routelist test_ipv4_routelist_reference.txt routelist_output_filter.sh

test_ipv4_routelist_coverage: test_ipv4_routelist
	test_ipv4_routelist -1 | grep Hit | awk '{print $$2}' | sort | uniq

# ----------

test_ipv6_routelist.o: ip_plugin.c
	cc -DTEST_IPV6_ROUTELIST ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o $@ $^

test_ipv6_routelist: test_ipv6_routelist.o IPMonitorControlPrefs.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o network_information_priv.o network_information_server.o smb-configuration.o proxy-configuration.o libSystemConfiguration_server.o IPMonitorControlServer.o
	cc -Wformat -Wall -O0 -g -o $@ $^ ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

test_ipv6_routelist_reference.txt: test_ipv6_routelist
	sh test_reference.sh create test_ipv6_routelist test_ipv6_routelist_reference.txt routelist_output_filter.sh

test_ipv6_routelist_test: test_ipv6_routelist
	sh test_reference.sh test test_ipv6_routelist test_ipv6_routelist_reference.txt routelist_output_filter.sh

test_ipv6_routelist_coverage: test_ipv6_routelist
	test_ipv6_routelist -1 | grep Hit | awk '{print $$2}' | sort | uniq

# ----------

IPMonitor.o: ip_plugin.c
	cc -DTEST_IPMONITOR ${TEST_INCLUDE} ${EXTRA_CFLAGS} -Wall -O0 -g -c -o IPMonitor.o ip_plugin.c

IPMonitor: IPMonitor.o IPMonitorControlPrefs.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o network_information_priv.o network_information_server.o proxy-configuration.o smb-configuration.o libSystemConfiguration_client.o libSystemConfiguration_server.o
	cc -Wall -O0 -g -o IPMonitor IPMonitor.o IPMonitorControlPrefs.o dnsinfo_create.o dnsinfo_flatfile.o dnsinfo_server.o network_information_priv.o network_information_server.o proxy-configuration.o smb-configuration.o libSystemConfiguration_client.o libSystemConfiguration_server.o ${EXTRA_CFLAGS} -framework SystemConfiguration -framework CoreFoundation

# ----------

clean:
	rm -rf *.dSYM *.o test_dns test_hostname test_proxy test_smb test_ipv4_routelist test_ipv6_routelist IPMonitor
