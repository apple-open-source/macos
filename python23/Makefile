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
Install_Target = frameworkinstallstructure libinstall libainstall sharedinstall oldsharedinstall
FIX = $(SRCROOT)/fix

##---------------------------------------------------------------------
# Patch Makefiles and pyconfig.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) ed - ${OBJROOT}/Makefile < $(FIX)/Makefile.ed
	$(_v) ed - ${OBJROOT}/pyconfig.h < $(FIX)/pyconfig.ed
	$(_v) patch ${OBJROOT}/Lib/plat-mac/applesingle.py \
		$(FIX)/applesingle.py.patch
	$(_v) $(TOUCH) $(ConfigStamp2)

##---------------------------------------------------------------------
# Fixup a lot of problems after the install
##---------------------------------------------------------------------
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVER = $(PYFRAMEWORK)/Versions/$(VERS)
RESOURCESVERS = $(VERSIONSVER)/Resources
LIBPYTHONVERS = $(VERSIONSVER)/lib/python$(VERS)
RUNPYTHON = DYLD_FRAMEWORK_PATH=$(OBJROOT) $(OBJROOT)/python.exe
BYTE2UTF16 = $(RUNPYTHON) $(FIX)/byte2utf16.py
UTF162BYTE = $(RUNPYTHON) $(FIX)/utf162byte.py

fixup-after-install: delete-stuff \
		     strip-installed-files \
		     fix-empty-file \
		     fix-CFBundleIdentifier \
		     fix-CFBundleShortVersionString \
		     fix-paths \
		     make-Library-Python \
		     fix-permissions \
		     fix-LINKFORSHARED

delete-stuff:
	rm -rf $(DSTROOT)/usr/local
	rm -rf $(DSTROOT)$(PYFRAMEWORK)/Headers
	rm -rf $(DSTROOT)$(PYFRAMEWORK)/Python
	rm -rf $(DSTROOT)$(PYFRAMEWORK)/Resources
	rm -rf $(DSTROOT)$(PYFRAMEWORK)/Versions/Current
	rm -rf $(DSTROOT)$(VERSIONSVER)/Headers
	rm -rf $(DSTROOT)$(VERSIONSVER)/bin

strip-installed-files:
	strip -x $(DSTROOT)$(VERSIONSVER)/Python
	strip -x $(DSTROOT)$(LIBPYTHONVERS)/config/python.o
	strip -x $(DSTROOT)$(LIBPYTHONVERS)/lib-dynload/*.so

fix-empty-file:
	echo '#' > $(DSTROOT)$(LIBPYTHONVERS)/bsddb/test/__init__.py
	$(RUNPYTHON) $(OBJROOT)/Lib/py_compile.py $(DSTROOT)$(LIBPYTHONVERS)/bsddb/test/__init__.py
	$(RUNPYTHON) -O $(OBJROOT)/Lib/py_compile.py $(DSTROOT)$(LIBPYTHONVERS)/bsddb/test/__init__.py

fix-CFBundleIdentifier:
	ed - $(DSTROOT)$(RESOURCESVERS)/Info.plist < $(FIX)/pfinfo.ed
 
fix-CFBundleShortVersionString:
	@set -x && \
	for s in `find $(DSTROOT)$(RESOURCESVERS) -name InfoPlist.strings`; do \
	    $(UTF162BYTE) $$s $(OBJROOT)/temp.ip.strings && \
	    ed - $(OBJROOT)/temp.ip.strings < $(FIX)/removeCFkeys.ed && \
	    $(BYTE2UTF16) $(OBJROOT)/temp.ip.strings $$s; \
	done

##---------------------------------------------------------------------
# adjustSLF.ed removes -arch xxx flags.
##---------------------------------------------------------------------
fix-paths:
	ed - $(DSTROOT)$(LIBPYTHONVERS)/config/Makefile < $(FIX)/adjustSLF.ed

# 4144521
fix-LINKFORSHARED:
	ed - $(DSTROOT)$(LIBPYTHONVERS)/config/Makefile < $(FIX)/LINKFORSHARED.ed

LIBRARYPYTHON = /Library/Python
LIBRARYPYTHONVERS = $(LIBRARYPYTHON)/$(VERS)
ORIGSITEPACKAGES = $(LIBRARYPYTHONVERS)/site-packages
SITEPACKAGES = $(LIBPYTHONVERS)/site-packages

make-Library-Python:
	install -d $(DSTROOT)$(LIBRARYPYTHONVERS)
	mv -f $(DSTROOT)$(SITEPACKAGES) $(DSTROOT)$(LIBRARYPYTHONVERS)
	ln -sf ../../../../../../../..$(ORIGSITEPACKAGES) $(DSTROOT)$(SITEPACKAGES)

fix-permissions:
	@set -x && for i in Library; do \
	    chgrp -Rf admin $(DSTROOT)/$$i && \
	    chmod -Rf g+w $(DSTROOT)/$$i; \
	done
