NAME = py2app
VERSION = 0.4.1.dev-r487

include ../Makefile.inc

custompatching:
	echo >> $(DIR)/$(NAME).egg-info/not-zip-safe
	ed - $(DIR)/py2app/__init__.py < __init__.py.ed
	ed - $(DIR)/py2app/bootstrap/site_packages.py < site_packages.py.ed
	rm -f $(DIR)/py2app/apptemplate/prebuilt/*
	cp -p $(DIR)/py2app/apptemplate/setup.py $(DIR)/py2app/apptemplate/setup.py.bak
	sed 's/XXXCFLAGSXXX/$(RC_CFLAGS)/' apptemplate_setup.py.ed | \
	    ed - $(DIR)/py2app/apptemplate/setup.py
	patch $(DIR)/py2app/apptemplate/src/main.c main.c.patch
	cd $(DIR)/py2app/apptemplate && python setup.py
	mv -f $(DIR)/py2app/apptemplate/setup.py.bak $(DIR)/py2app/apptemplate/setup.py
	rm -f $(DIR)/py2app/bundletemplate/prebuilt/*
	cp -p $(DIR)/py2app/bundletemplate/setup.py $(DIR)/py2app/bundletemplate/setup.py.bak
	sed 's/XXXCFLAGSXXX/$(RC_CFLAGS)/' bundletemplate_setup.py.ed | \
	    ed - $(DIR)/py2app/bundletemplate/setup.py
	cd $(DIR)/py2app/bundletemplate && python setup.py
	mv -f $(DIR)/py2app/bundletemplate/setup.py.bak $(DIR)/py2app/bundletemplate/setup.py
	ed - $(DIR)/py2app/build_app.py < build_app.py.ed

postinstall:
	mv $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main.64-bit
	ditto -arch ppc -arch i386 $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main.64-bit $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main
	mv $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main.64-bit
	ditto -arch ppc -arch i386 $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main.64-bit $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main
