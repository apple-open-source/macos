#
# RC Makefile for ypinit scripts on Rhapsody.
#
# $Id: Makefile,v 1.1.1.1 1999/05/02 03:59:01 wsanchez Exp $
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
	cp Makefile Makefile.yp Makefile.main ypinit.sh $(SRCROOT)$(SRCPATH)

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

