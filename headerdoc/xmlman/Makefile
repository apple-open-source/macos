
DEBUG_CFLAGS=
# DEBUG_CFLAGS="-g"

# LOCALCFLAGS= -I/usr/local/include/libxml2 
LOCALCFLAGS=
# LOCALLDFLAGS= -L/usr/local/lib 
LOCALLDFLAGS=

CFLAGS= ${LOCALCFLAGS} -I/usr/include/libxml2 -Wall ${RC_CFLAGS} ${DEBUG_CFLAGS} -mdynamic-no-pic
LDFLAGS= ${LOCALLDFLAGS} -lxml2 -force_flat_namespace

all: xml2man hdxml2manxml resolveLinks
	if [ "X${DEBUG_CFLAGS}" == "X" ] ; then \
		echo "Stripping"; \
		strip xml2man ; \
		strip hdxml2manxml ; \
		strip resolveLinks ; \
	fi

clean:
	rm -f xml2man hdxml2manxml resolveLinks

xml2man: xml2man.c
hdxml2manxml: hdxml2manxml.c
resolveLinks: resolveLinks.c

