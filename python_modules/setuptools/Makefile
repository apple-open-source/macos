NAME = setuptools
VERSION = 0.6c12dev-r85381

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

postinstall:
	chmod -x $(DSTROOT)$(EXTRASPYTHON)/setuptools-$(subst -,_,$(VERSION))*.egg-info/entry_points.txt
	cp -f PSFL.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
