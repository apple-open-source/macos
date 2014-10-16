NAME = setuptools
VERSION = 0.6c9

SETUPARGS = --install-scripts=/usr/bin

include ../Makefile.inc

custompatching:
	eval `sw_vers -productVersion | sed 's/^\([^.]*\)\.\([^.]*\).*/XXXMAJORXXX=\1 XXXMINORXXX=\2/'` && sed -e "s/XXXMAJORXXX/$$XXXMAJORXXX/" -e "s/XXXMINORXXX/$$XXXMINORXXX/" pkg_resources.py.ed | ed - $(DIR)/pkg_resources.py

postinstall:
	chmod -x $(DSTROOT)$(EXTRASPYTHON)/setuptools-$(VERSION)*.egg-info/entry_points.txt
	cp -f PSFL.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
