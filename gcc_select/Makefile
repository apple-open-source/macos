#
## B & I Makefile for gcc_select
#
# Copyright Apple Computer, Inc. 2002, 2003

#---------------------------------------------------------------------#

# Set V to what we wish to switch to when this project is "built".
# Note, you can set V from the buildit command line by specifying V=2
# or V=3[.x] after the pathname to the gcc_select source folder.

V = 3.3

#---------------------------------------------------------------------#

SRCROOT = .
SRC = `cd $(SRCROOT) && pwd | sed s,/private,,`

.PHONY: all install installhdrs installsrc installdoc clean

all: install

gcc_select.8: gcc_select.pod
	pod2man --section=8 --center="MacOS X" --release=`$(SRCROOT)/gcc_select --version | awk '{print $$2}'` gcc_select.pod gcc_select.8

# This install step does NOT switch the system compilers.  Instead it
# just sets up the desired sym links in $(DSTROOT)/usr.  It does, of
# course install gcc_select into /usr/sbin.  We depend on B & I to
# install the sym links from our $(DSTROOT)/usr into the system at
# the "proper" time.  Likewise, install the crufty cpp script into
# $(DSTROOT)/usr/bin.

install: installdoc
	$(SRCROOT)/gcc_select $(V) --dstroot $(DSTROOT)/usr
	mkdir -p $(DSTROOT)/usr/sbin && \
	install -c -m 555 $(SRCROOT)/gcc_select $(DSTROOT)/usr/sbin/gcc_select && \
	install -c -m 555 $(SRCROOT)/cpp $(DSTROOT)/usr/bin/cpp && \
	$(CC) $(SRCROOT)/c99.c -Wall -Werror -Os $(RC_CFLAGS) -o $(OBJROOT)/c99 && \
	strip $(OBJROOT)/c99 && \
	install -c -m 555 $(OBJROOT)/c99 $(DSTROOT)/usr/bin/c99 && \
	$(CC) $(SRCROOT)/c89.c -Os $(RC_CFLAGS) -o $(OBJROOT)/c89 && \
	strip $(OBJROOT)/c89 && \
	install -c -m 555 $(OBJROOT)/c89 $(DSTROOT)/usr/bin/c89

installsrc:
	if [ $(SRCROOT) != . ]; then  \
	    cp Makefile gcc_select cpp c99.c c89.c c99.1 c89.1 gcc_select.pod $(SRCROOT); \
	fi

installdoc: gcc_select.8
	mkdir -p $(DSTROOT)/usr/share/man/man1 && \
	mkdir -p $(DSTROOT)/usr/share/man/man8 && \
	install -c -m 444 $(SRCROOT)/c99.1 $(DSTROOT)/usr/share/man/man1/c99.1 && \
	install -c -m 444 $(SRCROOT)/c89.1 $(DSTROOT)/usr/share/man/man1/c89.1 && \
	install -c -m 444 $(SRCROOT)/gcc_select.8 $(DSTROOT)/usr/share/man/man8

installhdrs:
clean:
	rm -f gcc_select.8
