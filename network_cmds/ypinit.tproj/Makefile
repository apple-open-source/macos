#
# RC Makefile for ypinit scripts on Rhapsody.
#
# $Id: Makefile,v 1.4 2003/02/08 03:46:33 mscopp Exp $
#

VARYP=$(DSTROOT)/private/var/yp
USRSBIN=$(DSTROOT)/usr/sbin

PROJECT=ypinit.tproj

.PHONY: all install installhdrs installsrc build clean recurse

all: build

##
# Targets
##
	
build: build_init

build_init:
	@echo "Building $(PROJECT)..."

installsrc:
	-mkdir -p $(SRCROOT)$(SRCPATH)
	cp Makefile Makefile.yp Makefile.main ypinit.sh ypinit.8 $(SRCROOT)$(SRCPATH)

installhdrs:

clean:

install: install_makefiles install_binaries

install_makefiles:
	-mkdir -p $(VARYP)
	install -c -m 644 Makefile.yp $(VARYP)
	-chown root.wheel $(VARYP)/Makefile.yp
	install -c -m 644 Makefile.main $(VARYP)
	-chown root.wheel $(VARYP)/Makefile.main
	
install_binaries:
	-mkdir -p $(USRSBIN)
	install -c -m 755 ypinit.sh $(USRSBIN)
	-chown root.wheel $(USRSBIN)/ypinit
	-mkdir -p "$(DSTROOT)/usr/share/man/man8"
	install -c -m 644 ypinit.8 "$(DSTROOT)/usr/share/man/man8/ypinit.sh.8"

