dhcphosts: DHCPServer.c DHCPServer.h
	cc	-Wall					\
		-g					\
		-fconstant-cfstrings			\
		-DTEST_DHCPHOSTLIST			\
		-I../bootplib				\
		-o dhcphosts				\
		DHCPServer.c				\
		-framework CoreFoundation

clean:
	rm -f dhcphosts

