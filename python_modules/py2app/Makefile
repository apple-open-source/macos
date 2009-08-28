NAME = py2app
VERSION = 0.4.2

include ../Makefile.inc

custompatching:
	ed - $(DIR)/py2app/bootstrap/site_packages.py < site_packages.py.ed
	rm -f $(DIR)/py2app/apptemplate/prebuilt/*
	sed "s/###PRODUCTVERSION###/`sw_vers -productVersion`/" template_setup.py.ed | ed - $(DIR)/py2app/apptemplate/setup.py
	cd $(DIR)/py2app/apptemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	rm -f $(DIR)/py2app/bundletemplate/prebuilt/*
	sed "s/###PRODUCTVERSION###/`sw_vers -productVersion`/" template_setup.py.ed | ed - $(DIR)/py2app/bundletemplate/setup.py
	cd $(DIR)/py2app/bundletemplate && ARCHFLAGS='$(RC_CFLAGS)' python setup.py
	ed - $(DIR)/py2app/build_app.py < build_app.py.ed

postinstall:
ifeq "$(NO64)" "YES"
	mv $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main.64-bit
	ditto -arch ppc -arch i386 $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main.64-bit $(DSTROOT)$(EXTRASPYTHON)/py2app/apptemplate/prebuilt/main
	mv $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main.64-bit
	ditto -arch ppc -arch i386 $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main.64-bit $(DSTROOT)$(EXTRASPYTHON)/py2app/bundletemplate/prebuilt/main
endif
	cp -f $(DIR)/LICENSE.txt $(OSL)/$(NAME).txt
	cp -f $(NAME).partial $(OSV)
	install -d $(DSTROOT)$(EXTRASPYTHON)/ez_setup
	ditto $(DIR)/ez_setup/__init__.* $(DSTROOT)$(EXTRASPYTHON)/ez_setup
