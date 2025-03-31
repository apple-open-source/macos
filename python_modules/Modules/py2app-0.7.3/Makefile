NAME = py2app
VERSION = 0.7.3

include ../Makefile.inc

custompatching:
	sed -e 's,@LIBRARYPREFIX@,$(LIBRARYPREFIX),g' site_packages.py.ed | ed - $(DIR)/py2app/bootstrap/site_packages.py
	ed - $(DIR)/py2app/apptemplate/setup.py < template_setup.py.ed
	cd $(DIR)/py2app/apptemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	ed - $(DIR)/py2app/bundletemplate/setup.py < template_setup.py.ed
	cd $(DIR)/py2app/bundletemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	sed -e 's/@TOPDIR@/$(TOPDIR)/g' -e 's/@PYFRAMEWORKNAME@/$(PYFRAMEWORKNAME)/g' build_app.py.ed | ed - $(DIR)/py2app/build_app.py
	sed -e 's/@TOPDIR@/$(TOPDIR)/g' -e 's/@PYFRAMEWORKNAME@/$(PYFRAMEWORKNAME)/g' system_path_extras.py.ed | ed - $(DIR)/py2app/bootstrap/system_path_extras.py
	sed -e 's/@TOPDIR@/$(TOPDIR)/g' -e 's/@PYAPPNAME@/$(PYAPPNAME)/g' plist_template.py.ed | ed - $(DIR)/py2app/apptemplate/plist_template.py
	sed -e 's/@TOPDIR@/$(TOPDIR)/g' -e 's/@PYAPPNAME@/$(PYAPPNAME)/g' plist_template.py.ed | ed - $(DIR)/py2app/bundletemplate/plist_template.py
	ed - $(DIR)/setup.py < setup.py.ed

postinstall:
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME)-$(VERSION).txt
	cp -f $(NAME).partial $(OSV)/$(NAME)-$(VERSION).partial
	install -m 0755 -g 0 $(DIR)/py2app/apptemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt
	install -m 0755 -g 0 $(DIR)/py2app/bundletemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt
