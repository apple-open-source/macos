include ../platform-settings

CFLAGS=$(CCFLAGS) -I../jabberd/

dnsrv_HEADERS=srv_resolv.h ../jabberd/jabberd.h

dnsrv_OBJECTS=dnsrv.o srv_resolv.o

single: static

all: dnsrv_build

$(dnsrv_OBJECTS): $(dnsrv_HEADERS)

dnsrv_build: $(dnsrv_OBJECTS) $(dnsrv_HEADERS)
	$(CC) $(CFLAGS) $(MCFLAGS) -o dnsrv.so $(dnsrv_OBJECTS) 

static: $(dnsrv_OBJECTS) $(dnsrv_HEADERS)

clean:
	rm -f $(dnsrv_OBJECTS) dnsrv.so
