test_dns: Makefile dns-configuration.h dns-configuration.c
	cc -Wall -g -o test_dns -DMAIN dns-configuration.c -framework SystemConfiguration -framework CoreFoundation -ldnsinfo

test_smb: Makefile smb-configuration.h smb-configuration.c
	cc -Wall -g -o test_smb -DMAIN -DDEBUG smb-configuration.c -framework SystemConfiguration -framework CoreFoundation

test_ipv4_routelist: ip_plugin.c
	cc -Wall -g -o test_ipv4_routelist -DTEST_IPV4_ROUTELIST smb-configuration.c ip_plugin.c -framework SystemConfiguration -framework CoreFoundation -ldnsinfo

IPMonitor: ip_plugin.c
	cc -Wall -g -o IPMonitor -DTEST_IPMONITOR smb-configuration.c ip_plugin.c -framework SystemConfiguration -framework CoreFoundation -ldnsinfo

test_ipv4_routelist_reference.txt: test_ipv4_routelist
	sh test_reference.sh create test_ipv4_routelist test_ipv4_routelist_reference.txt test_ipv4_routelist_filter.sh

test_ipv4_routelist_test: test_ipv4_routelist
	sh test_reference.sh test test_ipv4_routelist test_ipv4_routelist_reference.txt test_ipv4_routelist_filter.sh


clean:
	rm -rf	test_dns test_dns.dSYM	\
		test_smb test_smb.dSYM	\
		test_ipv4_routelist test_ipv4_routelist.dSYM	\
		IPMonitor IPMonitor.dSYM
