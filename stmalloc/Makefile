# stmalloc makefile

AR = /usr/bin/ar
CC = /usr/bin/cc
CD = cd
CHMOD = /bin/chmod
CHOWN = /usr/sbin/chown
CP = /bin/cp
ECHO = /bin/echo
MKDIRS = /bin/mkdir -p
RANLIB = /usr/bin/ranlib -s
RM = /bin/rm -f
SYMLINK = /bin/ln -s

CFLAGS = -O3 -fno-common -pipe -Wall -I. -I$(SRCROOT) $(RC_CFLAGS)

## Default variable values - `make` builds in place

ifndef VERSION_NAME
VERSION_NAME = A
endif

ifndef SRCROOT
SRCROOT = .
endif

ifndef OBJROOT
OBJROOT = $(SRCROOT)
endif

ifndef SYMROOT
SYMROOT = $(SRCROOT)
endif

ifndef DSTROOT
DSTROOT = /
endif

ifndef INSTALLDIR
INSTALLDIR = /usr/lib
endif


## Targets
#    build		builds the library to OBJROOT and SYMROOT
#    installsrc		copies the sources to SRCROOT
#    installhdrs	install only the headers to DSTROOT
#    install		build, then install the headers and library to DSTROOT
#    clean		removes build products in OBJROOT and SYMROOT
#
#    optimized          same as 'build' but builds optimized library only
#    debug              same as 'build' but builds debug library only
#    profile            same as 'build' but builds profile library only


default: build
all: build
optimized: build
debug: build
profile: build

installsrc:
ifneq "$(SRCROOT)" "."
	$(CP) Makefile stmalloc.c stmalloc.h $(SRCROOT)
endif

installhdrs:
# no public headers to install

# dynamic library not installed by default
install: build installhdrs
	$(MKDIRS) $(DSTROOT)$(INSTALLDIR)
	-$(CHMOD) +w $(DSTROOT)$(INSTALLDIR)
	$(CP) $(SYMROOT)/libstmalloc.a $(DSTROOT)$(INSTALLDIR)/
	-$(CHOWN) root:wheel $(DSTROOT)$(INSTALLDIR)/libstmalloc.a
	$(CHMOD) 755 $(DSTROOT)$(INSTALLDIR)/libstmalloc.a

# dynamic library not built by default
build: static

dynamic: $(SYMROOT)/libstmalloc.$(VERSION_NAME).dylib
static: $(SYMROOT)/libstmalloc.a

$(SYMROOT)/libstmalloc.$(VERSION_NAME).dylib : $(SRCROOT)/stmalloc.c $(SRCROOT)/stmalloc.h
	$(CC) -c $(CFLAGS) \
          -dynamiclib \
          -install_name $(INSTALLDIR)/libstmalloc.$(VERSION_NAME).dylib \
          -o $(SYMROOT)/libstmalloc.$(VERSION_NAME).dylib \
          $(SRCROOT)/stmalloc.c

$(SYMROOT)/libstmalloc.a: $(SRCROOT)/stmalloc.c $(SRCROOT)/stmalloc.h
	$(CC) -c $(CFLAGS) \
          -mdynamic-no-pic \
          -o $(OBJROOT)/libstmalloc.o \
          $(SRCROOT)/stmalloc.c
	$(AR) -r $(SYMROOT)/libstmalloc.a $(OBJROOT)/libstmalloc.o
	$(RANLIB) $(SYMROOT)/libstmalloc.a

clean:
	$(RM) $(SYMROOT)/libstmalloc.a $(SYMROOT)/libstmalloc.$(VERSION_NAME).dylib $(OBJROOT)/libstmalloc.o 


.SUFFIXES:
.PHONY: default all optimized debug profile installsrc installhdrs install build static dynamic clean
