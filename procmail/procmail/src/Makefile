#$Id: Makefile,v 1.1.1.2 2001/07/20 19:38:17 bbraun Exp $

all: init
	$(MAKE) make $@

# The only real thing that can be made right now is:

init:
	cd ..; $(MAKE) make init

.PRECIOUS: Makefile

Makefile makefile Makefiles makefiles: init

procmail lockfile formail multigram mailstat setid getparams gethome \
../autoconf.h autoconf.h: init
	$(MAKE) make $@
