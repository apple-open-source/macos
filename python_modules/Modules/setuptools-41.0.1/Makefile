NAME = setuptools
VERSION = 41.0.1

SETUPARGS = --install-scripts=$(BINDIR)

include ../Makefile.inc

custompatching:
	sed -e 's,@PYEXECUTABLE@,$(PYEXECUTABLE),g' easy_install.py.patch | patch $(DIR)/setuptools/command/easy_install.py
	sed -e 's/@PYFRAMEWORKNAME@/$(PYFRAMEWORKNAME)/g' easy_install.py.ed | ed - $(DIR)/setuptools/command/easy_install.py

postinstall:
	chmod -x $(DSTROOT)$(EXTRASPYTHON)/setuptools-$(subst -,_,$(VERSION))*.egg-info/entry_points.txt
	cp -f PSFL.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
