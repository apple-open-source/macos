include ../platform-settings

CFLAGS=$(CCFLAGS) -I../jabberd

dialback_OBJECTS= \
	dialback.o \
	dialback_in.o \
	dialback_out.o

all: dialback

dialback: $(dialback_OBJECTS)
ifeq ($(__CYGWIN__),1)
	$(CC) $(CFLAGS) $(MCFLAGS) -o dialback.dll $(dialback_OBJECTS) ../jabberd/jabberd.a $(LDFLAGS) $(LIBS)
else
	$(CC) $(CFLAGS) $(MCFLAGS) -o dialback.so $(dialback_OBJECTS) $(LDFLAGS) $(LIBS)
endif


static: $(dialback_OBJECTS)

single: $(dialback_OBJECTS)

clean:
ifeq ($(__CYGWIN__),1)
	rm -f $(dialback_OBJECTS) dialback.dll
else
	rm -f $(dialback_OBJECTS) dialback.so
endif


.PHONY: clean
