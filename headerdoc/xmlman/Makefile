
DEBUG_CFLAGS=
# DEBUG_CFLAGS=-g -O0

# LOCALCFLAGS= -I/usr/local/include/libxml2 
LOCALCFLAGS=-Os
# LOCALLDFLAGS= -L/usr/local/lib 
LOCALLDFLAGS=

COMPATIBILITY_BITS=
VSTR=
ifneq ($(strip $(VERS)),)
	VSTR=-mmacosx-version-min=${VERS}
endif
ifeq (${ARCH},Darwin)
	DARWIN_CFLAGS=-mdynamic-no-pic ${VSTR}
	DARWIN_LDFLAGS=-force_flat_namespace ${VSTR}
else
	ifeq (${ARCH},Linux)
		DARWIN_CFLAGS= -DUSE_STRCOMPAT -D_GNU_SOURCE
		DARWIN_LDFLAGS=-lpthread
		# @echo "If you get duplicate symbol errors, remove -lstrinccompat from DARWIN_LDFLAGS."
		COMPATIBILITY_BITS=strcompat.o
	else
		DARWIN_CFLAGS=
		DARWIN_LDFLAGS=-lpthread
	endif
endif


CFLAGS= ${LOCALCFLAGS} -I/usr/include/libxml2 -Wall ${RC_CFLAGS} ${DEBUG_CFLAGS} ${DARWIN_CFLAGS}
LDFLAGS= ${LOCALLDFLAGS} -lxml2 ${DARWIN_LDFLAGS} ${RC_CFLAGS}

all: xml2man hdxml2manxml resolveLinks doc
	# echo "ARCH: ${ARCH}"
	if [ "X${DEBUG_CFLAGS}" = "X" ] ; then \
		echo "Stripping"; \
		strip xml2man ; \
		strip hdxml2manxml ; \
		strip resolveLinks ; \
	fi

clean:
	rm -f xml2man hdxml2manxml resolveLinks
	rm -f xml2man.o hdxml2manxml.o resolveLinks.o
	rm -f ../Documentation/man/hdxml2manxml.1
	rm -f ../Documentation/man/xml2man.1

xml2man: xml2man.o ${COMPATIBILITY_BITS}
hdxml2manxml: hdxml2manxml.o ${COMPATIBILITY_BITS}
resolveLinks: resolveLinks.o ${COMPATIBILITY_BITS}

doc: hdxml2manxml.mxml xml2man.mxml
	./xml2man -f hdxml2manxml.mxml ../Documentation/man/hdxml2manxml.1
	./xml2man -f xml2man.mxml ../Documentation/man/xml2man.1
	./xml2man -f manpages.mxml ../Documentation/man/manpages.5
