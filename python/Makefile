##
# Makefile for python
##

Project               = python
Extra_Configure_Flags = --enable-ipv6 --with-threads --enable-framework=/System/Library/Frameworks --enable-toolbox-glue

##---------------------------------------------------------------------
# Extra_CC_Flags and Extra_LD_Flags are needed because CFLAGS gets overridden
# by the RC_* variables.  These values would normally be set by the default
# python Makefile
#
# Workaround for 3281234 (test_coercion failure due to non IEEE-754 in
# optimizer): add -mno-fused-madd flag
##---------------------------------------------------------------------
Extra_CC_Flags += -fno-common -Wno-long-double -mno-fused-madd
Extra_LD_Flags += -Wl,-F.
Extra_Install_Flags   = DESTDIR=${DSTROOT}
GnuAfterInstall       = fixup-after-install

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags := $(shell echo $(Install_Flags) | sed 's/prefix=[^ ]* *//')
Install_Target = frameworkinstall
FIX = $(SRCROOT)/fix

##---------------------------------------------------------------------
# Patch Makefiles and pyconfig.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) ed - ${OBJROOT}/Makefile < $(FIX)/Makefile.ed
	$(_v) ed - ${OBJROOT}/Mac/OSX/Makefile < $(FIX)/OSXMakefile.ed
	$(_v) ed - ${OBJROOT}/pyconfig.h < $(FIX)/endian.ed
	$(_v) $(TOUCH) $(ConfigStamp2)

##---------------------------------------------------------------------
# Fixup a lot of problems after the install
##---------------------------------------------------------------------
VERS = 2.3
APPS = /Applications
DEVAPPS = /Developer/Applications/Utilities
USRBIN = /usr/bin
BUILDAPPLETNAME = BuildApplet.app
PYTHONAPPNAME = Python.app
PYTHONLAUNCHERNAME = PythonLauncher.app
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVER = $(PYFRAMEWORK)/Versions/$(VERS)
RESOURCESVERS = $(VERSIONSVER)/Resources
ENGLISHLPROJVERS = $(RESOURCESVERS)/English.lproj
LIBPYTHONVERS = $(VERSIONSVER)/lib/python$(VERS)
PYTHONAPP = $(RESOURCESVERS)/$(PYTHONAPPNAME)
PACONTENTS = $(PYTHONAPP)/Contents
PAMACOS = $(PACONTENTS)/MacOS
PARESOURCES = $(PACONTENTS)/Resources
PAENGLISHLPROJ = $(PARESOURCES)/English.lproj
MACPYTHON = $(APPS)/MacPython-$(VERS)
DEVMACPYTHON = $(DEVAPPS)/MacPython-$(VERS)
PYTHONLAUNCHER = $(RESOURCESVERS)/$(PYTHONLAUNCHERNAME)
PLCONTENTS = $(PYTHONLAUNCHER)/Contents
PLRESOURCES = $(PLCONTENTS)/Resources
PLENGLISHLPROJ = $(PLRESOURCES)/English.lproj
BUILDAPPLET = $(DEVMACPYTHON)/$(BUILDAPPLETNAME)
BACONTENTS = $(BUILDAPPLET)/Contents
BAMACOS = $(BACONTENTS)/MacOS
RUNPYTHON = DYLD_FRAMEWORK_PATH=$(OBJROOT) $(OBJROOT)/python.exe
BYTE2UTF16 = $(RUNPYTHON) $(FIX)/byte2utf16.py
UTF162BYTE = $(RUNPYTHON) $(FIX)/utf162byte.py

fixup-after-install: delete-stuff \
		     move-things-around \
		     strip-installed-files \
		     make-utf16 \
		     fix-empty-file \
		     fix-BAInfo \
		     fix-CFBundleIdentifier \
		     fix-CFBundleShortVersionString \
		     fix-paths \
		     fix-buildapplet \
		     fix-usr-local-bin \
		     make-usr-bin \
		     make-Library-Python \
		     fix-permissions

delete-stuff:
	rm -rf $(DSTROOT)/usr/local

move-things-around:
	install -d $(DSTROOT)$(DEVMACPYTHON)
	mv -f $(DSTROOT)$(MACPYTHON)/$(BUILDAPPLETNAME) $(DSTROOT)$(BUILDAPPLET)
	mv -f $(DSTROOT)$(MACPYTHON)/$(PYTHONLAUNCHERNAME) $(DSTROOT)$(RESOURCESVERS)
	rm -rf $(DSTROOT)$(APPS)

strip-installed-files:
	strip -x $(DSTROOT)$(VERSIONSVER)/Python
	strip -x $(DSTROOT)$(VERSIONSVER)/bin/python*
	strip -x $(DSTROOT)$(LIBPYTHONVERS)/config/python.o
	strip -x $(DSTROOT)$(LIBPYTHONVERS)/lib-dynload/*.so

make-utf16:
	@for i in $(DSTROOT)$(ENGLISHLPROJVERS) $(DSTROOT)$(PAENGLISHLPROJ); do \
	    echo mv $$i/InfoPlist.strings $$i/temp-ip.strings; \
	    mv $$i/InfoPlist.strings $$i/temp-ip.strings; \
	    echo $(BYTE2UTF16) $$i/temp-ip.strings $$i/InfoPlist.strings; \
	    $(BYTE2UTF16) $$i/temp-ip.strings $$i/InfoPlist.strings; \
	    echo rm -f $$i/temp-ip.strings; \
	    rm -f $$i/temp-ip.strings; \
	done

fix-empty-file:
	echo '#' > $(DSTROOT)$(LIBPYTHONVERS)/bsddb/test/__init__.py

fix-BAInfo:
	ed - $(DSTROOT)$(BACONTENTS)/Info.plist < $(FIX)/bainfo.ed

fix-CFBundleIdentifier:
	ed - $(DSTROOT)$(RESOURCESVERS)/Info.plist < $(FIX)/pfinfo.ed

fix-CFBundleShortVersionString:
	$(UTF162BYTE) $(DSTROOT)$(PLENGLISHLPROJ)/InfoPlist.strings $(DSTROOT)$(PLENGLISHLPROJ)/temp.ip.strings
	ed - $(DSTROOT)$(PLENGLISHLPROJ)/temp.ip.strings < $(FIX)/plsvs.ed
	$(BYTE2UTF16) $(DSTROOT)$(PLENGLISHLPROJ)/temp.ip.strings $(DSTROOT)$(PLENGLISHLPROJ)/InfoPlist.strings
	rm -f $(DSTROOT)$(PLENGLISHLPROJ)/temp.ip.strings
	$(UTF162BYTE) $(DSTROOT)$(ENGLISHLPROJVERS)/InfoPlist.strings $(DSTROOT)$(ENGLISHLPROJVERS)/temp.ip.strings
	ed - $(DSTROOT)$(ENGLISHLPROJVERS)/temp.ip.strings < $(FIX)/2.3svs.ed
	$(BYTE2UTF16) $(DSTROOT)$(ENGLISHLPROJVERS)/temp.ip.strings $(DSTROOT)$(ENGLISHLPROJVERS)/InfoPlist.strings
	rm -f $(DSTROOT)$(ENGLISHLPROJVERS)/temp.ip.strings
	$(UTF162BYTE) $(DSTROOT)$(PAENGLISHLPROJ)/InfoPlist.strings $(DSTROOT)$(PAENGLISHLPROJ)/temp.ip.strings
	ed - $(DSTROOT)$(PAENGLISHLPROJ)/temp.ip.strings < $(FIX)/pasvs.ed
	$(BYTE2UTF16) $(DSTROOT)$(PAENGLISHLPROJ)/temp.ip.strings $(DSTROOT)$(PAENGLISHLPROJ)/InfoPlist.strings
	rm -f $(DSTROOT)$(PAENGLISHLPROJ)/temp.ip.strings

PYDOC = $(USRBIN)/pydoc
PYDOCORIG = $(PYFRAMEWORK)/Versions/$(VERS)/bin/pydoc

##---------------------------------------------------------------------
# adjustSLF.ed removes -arch xxx flags.  fixusrbin.ed makes the exec
# path /usr/bin.
##---------------------------------------------------------------------
fix-paths:
	ed - $(DSTROOT)$(LIBPYTHONVERS)/config/Makefile < $(FIX)/adjustSLF.ed
	ed - $(DSTROOT)$(PYDOCORIG) < $(FIX)/fixusrbin.ed

fix-buildapplet:
	ed - $(DSTROOT)$(BAMACOS)/BuildApplet < $(FIX)/buildapplet.ed

fix-usr-local-bin:
	cd $(DSTROOT)$(VERSIONSVER) && patch -p0 < $(FIX)/usrlocalbin.patch
	@for i in `find $(DSTROOT)$(VERSIONSVER) -type f | xargs grep -l /usr/local/bin/python`; do \
	    echo ed - $$i \< $(FIX)/usrlocalbin.ed; \
	    ed - $$i < $(FIX)/usrlocalbin.ed; \
	done

make-usr-bin:
	install -d $(DSTROOT)$(USRBIN)
	ln -sf python$(VERS) $(DSTROOT)$(USRBIN)/python
	ln -sf ../../System/Library/Frameworks/Python.framework/Versions/$(VERS)/bin/python $(DSTROOT)$(USRBIN)/python$(VERS)
	ln -sf pythonw$(VERS) $(DSTROOT)$(USRBIN)/pythonw
	install -p $(FIX)/pythonw$(VERS) $(DSTROOT)$(USRBIN)
	install -p $(DSTROOT)$(PYDOCORIG) $(DSTROOT)$(PYDOC)

LIBRARYPYTHON = /Library/Python
LIBRARYPYTHONVERS = $(LIBRARYPYTHON)/$(VERS)
SITEPACKAGES = $(LIBPYTHONVERS)/site-packages

make-Library-Python:
	install -d $(DSTROOT)$(LIBRARYPYTHON)
	mv -f $(DSTROOT)$(SITEPACKAGES) $(DSTROOT)$(LIBRARYPYTHONVERS)
	ln -sf ../../../../../../../..$(LIBRARYPYTHONVERS) $(DSTROOT)$(SITEPACKAGES)

fix-permissions:
	@for i in Applications Developer Library; do \
	    echo chgrp -Rf admin $(DSTROOT)/$$i; \
	    chgrp -Rf admin $(DSTROOT)/$$i; \
	    echo chmod -Rf g+w $(DSTROOT)/$$i; \
	    chmod -Rf g+w $(DSTROOT)/$$i; \
	done
