dhcphosts: DHCPServer.c DHCPServer.h
	cc	-Wall					\
		-g					\
		-fconstant-cfstrings			\
		-DTEST_DHCPHOSTLIST			\
		-I../bootplib				\
		-o dhcphosts				\
		../bootplib/NICache.c			\
		../bootplib/netinfo.c			\
		DHCPServer.c				\
		-framework CoreFoundation

clean:
	rm -f dhcphosts

