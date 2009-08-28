#$Id: Makefile,v 1.9 2000/09/28 01:23:11 guenther Exp $

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
