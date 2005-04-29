#
# Ok this is taken from an automaked file and tweaked out
#
# Portions (c) Copyright 2005 Apple Computer, Inc.
#
include ../../platform-settings

CFLAGS:=$(CFLAGS)

lib_HEADERS= lib.h

lib_OBJECTS = \
    expat.o \
    base64.o \
	genhash.o \
	jid.o \
	jpacket.o \
	jutil.o \
	karma.o \
	pool.o \
	pproxy.o \
	rate.o \
	sha.o \
	snprintf.o \
	socket.o \
	str.o \
	xmlnode.o \
	xhash.o \
	xstream.o

$(lib_OBJECTS): $(lib_HEADERS)

single: static

static: $(lib_OBJECTS) $(lib_HEADERS)

all: $(lib_OBJECTS) $(lib_HEADERS)

clean:
	rm -f $(lib_OBJECTS) jabberd
