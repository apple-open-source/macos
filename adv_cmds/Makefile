THIS=$(shell basename `pwd` .tproj)

install:: obj all

all obj installhdrs installsrc clean ${THIS} install::
	env MAKEOBJDIRPREFIX=${OBJROOT} bsdmake COPTS="-Os -mdynamic-no-pic" ARCH_FLAGS="${RC_CFLAGS}" NOMANCOMPRESS=true DESTDIR=${DSTROOT} -f BSDmakefile $@

install::
	perl mksymroot ${DSTROOT} ${SYMROOT} ${OBJROOT}
