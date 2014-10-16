NAME = py2app
VERSIONORIG = 0.7.3
VERSION = $(VERSIONORIG)-noprebuilt

include ../Makefile.inc

afterinstallsrc::
	gnutar xozf $(NAME)-$(VERSIONORIG).tar.gz
	@set -x && \
	cd $(NAME)-$(VERSIONORIG) && \
	rm -rf examples/PyObjC/TinyTinyEdit/dist* && \
	rm -f py2app/apptemplate/prebuilt/* && \
	rm -f py2app/bundletemplate/prebuilt/*
	mv $(NAME)-$(VERSIONORIG) $(NAME)-$(VERSION)
	gnutar czf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	rm -f $(NAME)-$(VERSIONORIG).tar.gz
	rm -rf $(NAME)-$(VERSION)

custompatching:
	ed - $(DIR)/py2app/bootstrap/site_packages.py < site_packages.py.ed
	ed - $(DIR)/py2app/apptemplate/setup.py < template_setup.py.ed
	cd $(DIR)/py2app/apptemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	ed - $(DIR)/py2app/bundletemplate/setup.py < template_setup.py.ed
	cd $(DIR)/py2app/bundletemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	ed - $(DIR)/py2app/build_app.py < build_app.py.ed
	ed - $(DIR)/setup.py < setup.py.ed

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSIONORIG).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSIONORIG).partial
	ditto $(DIR)/py2app/apptemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt
	ditto $(DIR)/py2app/bundletemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt
