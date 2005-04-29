include ../platform-settings

CFLAGS=$(CCFLAGS) -I../jabberd

xdb_file_OBJECTS= xdb_file.o

single: static

all: xdb_file

xdb_file: $(xdb_file_OBJECTS)
ifeq ($(__CYGWIN__),1)
	$(CC) $(CFLAGS) $(MCFLAGS) -o xdb_file.dll $(xdb_file_OBJECTS) ../jabberd/jabberd.a $(LDFLAGS) $(LIBS)
else
	$(CC) $(CFLAGS) $(MCFLAGS) -o xdb_file.so $(xdb_file_OBJECTS) $(LDFLAGS) $(LIBS)
endif

static: $(xdb_file_OBJECTS)

clean:
ifeq ($(__CYGWIN__),1)
	rm -f $(xdb_file_OBJECTS) xdb_file.dll
else
	rm -f $(xdb_file_OBJECTS) xdb_file.so
endif
