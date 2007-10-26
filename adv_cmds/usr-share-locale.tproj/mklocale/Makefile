THIS=$(shell basename `pwd` .tproj)

install:: all

all installhdrs installsrc clean ${THIS} install::
	env MAKEOBJDIR=${OBJROOT} bsdmake -d msv ARCH_FLAGS="${RC_CFLAGS}" NO_MANCOMPRESS=true DESTDIR=${DSTROOT} -f BSDmakefile $@
