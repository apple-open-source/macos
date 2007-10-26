#$Id: Makefile,v 1.1 1999/09/23 17:30:07 wsanchez Exp $

all: init
	$(MAKE) make $@

# The only real thing that can be made right now is:

init:
	cd ..; $(MAKE) make init

.PRECIOUS: Makefile

Makefile makefile Makefiles makefiles: init

procmail.1 procmailrc.5 procmailsc.5 procmailex.5 lockfile.1 formail.1: init
	$(MAKE) make $@
