##
## $Id: Makefile,v 1.13 2003/08/10 19:39:04 rbraun Exp $
##

## Note, ideally I would prefer it if this top level makefile did
## not get created by autoconf. As I find typing 'make' and relying
## on it to take care of all dependencies much more friendly than
## the multi-stage autoconf+make and also worry about updates to
## configure.in not getting propagated down the tree. (AGM) [I realise
## that this may not prove possible, but at least I tried.. Sigh.]

DISTNAME=Linux-PAM

ifeq ($(shell test \! -f Make.Rules || echo yes),yes)
    include Make.Rules
endif

ifeq ($(GIANT_LIBPAM),yes)
THINGSTOMAKE = libpam libpamc libpam_misc giantlibpam doc modules
else
THINGSTOMAKE = libpam libpamc libpam_misc giantlibpam doc modules
endif

all: $(THINGSTOMAKE)

real-giantlibpam:
    ifeq ($(USESONAME),yes)
	$(LD_L) $(SOSWITCH) /usr/lib/libpam.$(MAJOR_REL).$(DYNTYPE) -o libpam/libpam.$(DYNTYPE) libpam/libpam.a libpamc/libpamc.a libpam_misc/libpam_misc.a
    else
	$(LD_L) -o libpam/libpam.$(DYNTYPE) libpam/libpam.a libpamc/libpamc.a libpam_misc/libpam_misc.a
    endif

prep:
	rm -f security
	ln -sf . security

clean:
	if [ ! -f Make.Rules ]; then touch Make.Rules ; fi
	for i in $(THINGSTOMAKE) ; do $(MAKE) -C $$i clean ; done
	rm -f security *~ *.orig *.rej Make.Rules #*#

distclean: clean
	rm -f Make.Rules 
	rm -f config.status config.cache config.log core

maintainer-clean: distclean
	@echo files should be ok for packaging now.

# NB _pam_aconf.h.in changes will remake this too
Make.Rules: Make.Rules.in _pam_aconf.h.in
	@echo XXX - not sure how to preserve past configure options..
	@echo XXX - so not attempting to. Feel free to run ./configure
	@echo XXX - by hand, with the options you want.
	./configure

_pam_aconf.h: Make.Rules

configure: configure.in
	@echo
	@echo You do not appear to have an up-to-date ./configure file.
	@echo Please run autoconf, and then ./configure [..options..]
	@echo
	@rm -f configure
	@exit 1

$(THINGSTOMAKE): _pam_aconf.h prep
	if [ "$@" = "giantlibpam" ]; then \
		$(MAKE) real-giantlibpam ; \
	else \
		$(MAKE) -C $@ all; \
	fi

install: _pam_aconf.h prep
	$(MKDIR) $(FAKEROOT)$(INCLUDED)
	$(INSTALL) -m 444 security/_pam_aconf.h $(FAKEROOT)$(INCLUDED)
	for x in $(THINGSTOMAKE) ; do $(MAKE) -C $$x install ; done

remove:
	rm -f $(FAKEROOT)$(INCLUDED)/_pam_aconf.h
	for x in $(THINGSTOMAKE) ; do $(MAKE) -C $$x remove ; done

release:
	@if [ ! -f Make.Rules ]; then echo $(MAKE) Make.Rules first ;exit 1 ;fi
	@if [ ! -L ../$(DISTNAME)-$(MAJOR_REL).$(MINOR_REL) ]; then \
	   echo generating ../$(DISTNAME)-$(MAJOR_REL).$(MINOR_REL) link ; \
	   ln -sf $(DISTNAME) ../$(DISTNAME)-$(MAJOR_REL).$(MINOR_REL) ; \
	   echo to ../$(DISTNAME) . ; fi
	@diff ../$(DISTNAME)-$(MAJOR_REL).$(MINOR_REL)/Make.Rules Make.Rules
	$(MAKE) distclean
	cd .. ; tar zvfc $(DISTNAME)-$(MAJOR_REL).$(MINOR_REL).tar.gz \
		--exclude CVS --exclude .cvsignore --exclude '.#*' \
		$(DISTNAME)-$(MAJOR_REL).$(MINOR_REL)/*

