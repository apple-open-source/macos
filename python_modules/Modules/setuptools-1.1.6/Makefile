NAME = setuptools
VERSION = 1.1.6

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

postinstall:
	chmod -x $(DSTROOT)$(EXTRASPYTHON)/setuptools-$(subst -,_,$(VERSION))*.egg-info/entry_points.txt
	cp -f PSFL.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
