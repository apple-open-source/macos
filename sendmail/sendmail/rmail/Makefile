#       $Id: Makefile,v 1.2 2000/07/25 04:43:52 wsanchez Exp $

SHELL= /bin/sh
BUILD=   ./Build
OPTIONS= $(CONFIG) $(FLAGS)

all: FRC
	$(SHELL) $(BUILD) $(OPTIONS) $@
clean: FRC
	$(SHELL) $(BUILD) $(OPTIONS) $@
install: FRC
	$(SHELL) $(BUILD) $(OPTIONS) $@
force-install: FRC
	$(SHELL) $(BUILD) $(OPTIONS) $@

strip:
	$(SHELL) $(BUILD) $(OPTIONS) strip-rmail

fresh: FRC
	$(SHELL) $(BUILD) $(OPTIONS) -c

FRC:
