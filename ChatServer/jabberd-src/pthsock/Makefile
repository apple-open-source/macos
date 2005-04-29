include ../platform-settings

CFLAGS=$(CCFLAGS) -I../jabberd/

HEADERS=../jabberd/jabberd.h

pthsock_client_OBJECTS= client.o

single: static

all: pthsock_client

$(pthsock_client_OBJECTS): $(HEADERS)

pthsock_client: $(pthsock_client_OBJECTS) $(HEADERS)
ifeq ($(__CYGWIN__),1)
	$(CC) $(CFLAGS) $(MCFLAGS) -o pthsock_client.dll $(pthsock_client_OBJECTS) ../jabberd/jabberd.a $(LDFLAGS) $(LIBS)
else
	$(CC) $(CFLAGS) $(MCFLAGS) -o pthsock_client.so $(pthsock_client_OBJECTS) $(LDFLAGS) $(LIBS)
endif

static: $(pthsock_client_OBJECTS) $(HEADERS)

clean:
ifeq ($(__CYGWIN__),1)
	rm -f $(pthsock_client_OBJECTS) pthsock_client.dll
else
	rm -f $(pthsock_client_OBJECTS) pthsock_client.so
endif
