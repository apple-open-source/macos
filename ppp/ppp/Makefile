#
# ppp top level makefile for Mac OS X
#

DSTDIR = 
BINDIR = $(DSTROOT)/usr/bin
MANDIR = $(DSTROOT)/usr/share/man
ETCDIR = $(DSTROOT)/private/etc/ppp
MODEMSDIR = $(DSTROOT)/private/etc/ppp/modems

all:
	#cd chat; $(MAKE) all
	cd pppd; $(MAKE) all
	#cd pppstats; $(MAKE) all
	#cd pppdump; $(MAKE) all

install: all $(BINDIR) $(MANDIR)/man8 install-progs install-etcppp

installsrc:
	cp -r * $(SRCROOT)

installhdrs::

install-progs:
	#cd chat; $(MAKE) install
	cd pppd; $(MAKE) install
	#cd pppstats; $(MAKE) install
	#cd pppdump; $(MAKE) install

#install-etcppp: $(ETCDIR) $(ETCDIR)/options $(ETCDIR)/pap-secrets \
#	$(ETCDIR)/chap-secrets
#install-etcppp: $(ETCDIR) $(MODEMSDIR) install-modems
install-etcppp: $(ETCDIR)

$(ETCDIR)/options:
	cp etc.ppp/options $@
	chmod go-w $@
$(ETCDIR)/pap-secrets:
	cp etc.ppp/pap-secrets $@
	chmod 600 $@
$(ETCDIR)/chap-secrets:
	cp etc.ppp/chap-secrets $@
	chmod 600 $@

$(BINDIR):
	mkdir -m 755 -p $@
$(MANDIR)/man8:
	mkdir -m 755 -p $@
$(ETCDIR):
	mkdir -m 755 -p $@
#$(MODEMSDIR):
#	mkdir -m 755 -p $@

#install-modems:
#	cp -f MacOSX/modems/HayesStandard $(MODEMSDIR)
#	cp -f MacOSX/modems/NullModem $(MODEMSDIR)
#	cp -f MacOSX/modems/AppleInternalModem56k $(MODEMSDIR)
#	cp -f MacOSX/modems/AppleInternalModem56k\(V34only\) $(MODEMSDIR)
#	cp -f MacOSX/modems/AppleGlobalVillage56k $(MODEMSDIR)


clean:
	rm -f *~
	#cd chat; $(MAKE) clean
	cd pppd; $(MAKE) clean
	#cd pppstats; $(MAKE) clean
	#cd pppdump; $(MAKE) clean





