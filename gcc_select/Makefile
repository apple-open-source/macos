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

.PHONY: all install installhdrs installsrc clean

all: install

# This install step does NOT switch the system compilers.  Instead it
# just sets up the desired sym links in $(DSTROOT)/usr.  It does, of
# course install gcc_select into /usr/sbin.  We depend on B & I to
# install the sym links from our $(DSTROOT)/usr into the system at
# the "proper" time.  Likewise, install the crufty cpp script into
# $(DSTROOT)/usr/bin.

install:
	$(SRCROOT)/gcc_select $(V) --dstroot $(DSTROOT)/usr
	mkdir -p $(DSTROOT)/usr/sbin && \
	install -c -m 555 $(SRCROOT)/gcc_select $(DSTROOT)/usr/sbin/gcc_select && \
	install -c -m 555 $(SRCROOT)/cpp $(DSTROOT)/usr/bin/cpp

installsrc:
	if [ $(SRCROOT) != . ]; then  \
	    cp Makefile gcc_select cpp $(SRCROOT); \
	fi

installhdrs:
clean:
