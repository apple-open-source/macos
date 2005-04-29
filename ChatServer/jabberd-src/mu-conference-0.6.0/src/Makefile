include ../../platform-settings
CFLAGS:=$(CFLAGS) -I../../jabberd -I../include  -I$(EXEC_PREFIX)/include/glib-2.0 -I$(EXEC_PREFIX)/include/glib-2.0/glib
#LIBS:=$(LIBS) -L$(PREFIX)/lib -L$(EXEC_PREFIX)/lib -lglib-2.0 -lintl -liconv
LIBS:= $(LIBS) $(EXEC_PREFIX)/lib/libglib-2.0.a $(EXEC_PREFIX)/lib/libintl.a 

#-I$(PREFIX)/include/glib-2.0 -I$(EXEC_PREFIX)/lib/glib-2.0/include
#-L$(EXEC_PREFIX)/lib -lglib-2.0 -lintl -liconv  

# Debug/Experimental
#CFLAGS:=$(CFLAGS) -pipe -Os -I../../jabberd -I../include 
#LIBS:=$(LIBS) /usr/local/lib/ccmalloc-gcc.o -lccmalloc 
#LIBS:=$(LIBS) -lmemusage
#LIBS:=$(LIBS) -lmcheck

conference_OBJECTS=conference.o conference_room.o conference_user.o utils.o xdata.o admin.o roles.o xdb.o hash.o

all: conference

conference: $(conference_OBJECTS)
ifeq ($(__CYGWIN__),1)
	$(CC) $(CFLAGS) $(MCFLAGS) -o mu-conference.dll $(conference_OBJECTS) ../../jabberd/jabberd.a $(LDFLAGS) $(LIBS)
else        
	$(CC) $(CFLAGS) $(MCFLAGS) -o mu-conference.so $(conference_OBJECTS) $(LDFLAGS) $(LIBS)
endif

static: $(conference_OBJECTS)

single: $(conference_OBJECTS)

clean:
ifeq ($(__CYGWIN__),1)
	rm -f $(conference_OBJECTS) mu-conference.dll
else        
	rm -f $(conference_OBJECTS) mu-conference.so *~
endif
