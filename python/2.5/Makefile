##
# Makefile for python
##

Project               = python
Extra_Configure_Flags = --enable-ipv6 --with-threads --enable-framework=/System/Library/Frameworks --enable-toolbox-glue --enable-dtrace --with-system-ffi --with-gcc=$(WITH_GCC) CC=$(WITH_GCC) CXX=$(WITH_GXX)

##---------------------------------------------------------------------
# Extra_CC_Flags and Extra_LD_Flags are needed because CFLAGS gets overridden
# by the RC_* variables.  These values would normally be set by the default
# python Makefile
#
# Workaround for 3281234 (test_coercion failure due to non IEEE-754 in
# optimizer): add -mno-fused-madd flag
##---------------------------------------------------------------------
Extra_CC_Flags += -fno-common -mno-fused-madd -DENABLE_DTRACE
Extra_LD_Flags += -Wl,-F.
Extra_Install_Flags   = DESTDIR='$(DSTROOT)'
GnuAfterInstall       = fixup-after-install install-plist
Extra_Environment     = CCSHARED='$(RC_CFLAGS)' EXTRA_CFLAGS='-DMACOSX -I/usr/include/ffi'

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags := $(shell echo $(Install_Flags) | sed 's/prefix=[^ ]* *//')
Install_Target = frameworkinstall
FIX = '$(SRCROOT)/fix'

##---------------------------------------------------------------------
# Patch Makefiles and pyconfig.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	ed - '$(OBJROOT)/Makefile' < $(FIX)/Makefile.ed
	ed - '$(OBJROOT)/pyconfig.h' < $(FIX)/pyconfig.ed
	$(TOUCH) $(ConfigStamp2)

##---------------------------------------------------------------------
# Fixup a lot of problems after the install
##---------------------------------------------------------------------
APPS = /Applications
DEVAPPS = /Developer/Applications/Utilities
USRBIN = /usr/bin
BUILDAPPLETNAME = Build Applet.app
PYTHONAPPNAME = Python.app
PYTHONLAUNCHERNAME = Python Launcher.app
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVERS = $(PYFRAMEWORK)/Versions/$(VERS)
RESOURCESVERS = $(VERSIONSVERS)/Resources
LIBPYTHONVERS = $(VERSIONSVERS)/lib/python$(VERS)
PYTHONAPP = $(RESOURCESVERS)/$(PYTHONAPPNAME)
PACONTENTS = $(PYTHONAPP)/Contents
PAMACOS = $(PACONTENTS)/MacOS
MACPYTHON = $(APPS)/MacPython $(VERS)
DEVMACPYTHON = $(DEVAPPS)/MacPython $(VERS)
PYTHONLAUNCHER = $(RESOURCESVERS)/$(PYTHONLAUNCHERNAME)
PLCONTENTS = $(PYTHONLAUNCHER)/Contents
BUILDAPPLET = $(DEVMACPYTHON)/$(BUILDAPPLETNAME)
BACONTENTS = $(BUILDAPPLET)/Contents
BAMACOS = $(BACONTENTS)/MacOS
RUNPYTHON = DYLD_FRAMEWORK_PATH='$(OBJROOT)' '$(OBJROOT)/python.exe'
BYTE2UTF16 = $(RUNPYTHON) $(FIX)/byte2utf16.py
UTF162BYTE = $(RUNPYTHON) $(FIX)/utf162byte.py

fixup-after-install: delete-stuff \
		     move-things-around \
		     strip-64-bit \
		     strip-installed-files \
		     fix-BAInfo \
		     fix-PAInfo \
		     fix-PyInfo \
		     fix-CFBundleShortVersionString \
		     fix-paths \
		     fix-usr-local-bin \
		     fix-usr-bin \
		     fix-permissions \
		     fix-config-Makefile \
		     additional-man-pages

delete-stuff:
	rm -rf '$(DSTROOT)/usr/local'

move-things-around:
	install -d '$(DSTROOT)$(DEVMACPYTHON)'
	mv -f '$(DSTROOT)$(MACPYTHON)/$(BUILDAPPLETNAME)' '$(DSTROOT)$(BUILDAPPLET)'
	mv -f '$(DSTROOT)$(MACPYTHON)/$(PYTHONLAUNCHERNAME)' '$(DSTROOT)$(RESOURCESVERS)'
	rm -rf '$(DSTROOT)$(APPS)'

##---------------------------------------------------------------------
# Force python, pythonw and Python Launcher to be 32-bit only
# (python will have previously been forced)
##---------------------------------------------------------------------
Python_Launcher = $(DSTROOT)$(PLCONTENTS)/MacOS/Python Launcher
PYTHON = $(DSTROOT)$(VERSIONSVERS)/bin/python$(VERS)
PYTHONW = $(DSTROOT)$(VERSIONSVERS)/bin/pythonw$(VERS)
strip-64-bit:
	@set -x && a='' && \
	for i in '$(PYTHON)' '$(PYTHONW)' '$(Python_Launcher)'; do \
	    ditto "$$i" $(SYMROOT) || exit 1; \
	done && \
	for i in ppc64 x86_64; do \
	    lipo '$(PYTHONW)' -verify_arch $$i && a="$$a -remove $$i"; \
	done; \
	test -z "$$a" || \
	for i in '$(PYTHON)' '$(PYTHONW)' '$(Python_Launcher)'; do \
	    lipo  "$$i" $$a -output "$$i" || exit 1; \
	done

strip-installed-files:
	$(CP) '$(DSTROOT)$(VERSIONSVERS)/Python' '$(SYMROOT)'
	strip -x '$(DSTROOT)$(VERSIONSVERS)/Python'
	strip -x '$(Python_Launcher)'
	ditto '$(DSTROOT)$(LIBPYTHONVERS)'/lib-dynload/*.so '$(SYMROOT)/lib-dynload/'
	strip -x '$(DSTROOT)$(LIBPYTHONVERS)'/lib-dynload/*.so

fix-BAInfo:
	sed 's/==VERSION==/$(PYTHON_CURRENT_VERSION)/' $(FIX)/bainfo.ed \
	    | ed - '$(DSTROOT)$(BACONTENTS)/Info.plist'

fix-PAInfo:
	sed 's/==VERSION==/$(PYTHON_CURRENT_VERSION)/' $(FIX)/painfo.ed \
	    | ed - '$(DSTROOT)$(PACONTENTS)/Info.plist'

fix-PyInfo:
	sed 's/==VERSION==/$(PYTHON_CURRENT_VERSION)/' $(FIX)/pyinfo.ed \
	    | ed - '$(DSTROOT)$(RESOURCESVERS)/Info.plist'

fix-CFBundleShortVersionString:
	@set -x && \
	cd '$(DSTROOT)$(RESOURCESVERS)' && \
	for s in `find . -name InfoPlist.strings`; do \
	    $(UTF162BYTE) "$$s" '$(OBJROOT)/temp.ip.strings' && \
	    ed - '$(OBJROOT)/temp.ip.strings' < $(FIX)/removeCFkeys.ed && \
	    $(BYTE2UTF16) '$(OBJROOT)/temp.ip.strings' "$$s"; \
	done

MAN1 = /usr/share/man/man1
additional-man-pages:
	install -m 0644 $(FIX)/pydoc.1 '$(DSTROOT)$(MAN1)'
	install -m 0644 $(FIX)/pythonw.1 '$(DSTROOT)$(MAN1)'
	ln -sf pydoc.1 '$(DSTROOT)$(MAN1)/pydoc$(VERS).1'
	ln -sf python.1 '$(DSTROOT)$(MAN1)/python$(VERS).1'
	ln -sf pythonw.1 '$(DSTROOT)$(MAN1)/pythonw$(VERS).1'

PYDOC = $(USRBIN)/pydoc
PYDOCORIG = $(VERSIONSVERS)/bin/pydoc

##---------------------------------------------------------------------
# fixusrbin.ed makes the exec path /usr/bin.
##---------------------------------------------------------------------
fix-paths:
	ed - '$(DSTROOT)$(PYDOCORIG)' < $(FIX)/fixusrbin.ed

CGIPY = $(LIBPYTHONVERS)/cgi.py
fix-usr-local-bin:
	@set -x && \
	cd '$(DSTROOT)$(VERSIONSVERS)' && \
	patch -p0 < $(FIX)/usrlocalbin.patch && \
	$(RUNPYTHON) -c "from py_compile import compile;compile('$(DSTROOT)$(CGIPY)', dfile='$(CGIPY)', doraise=True)" && \
	$(RUNPYTHON) -O -c "from py_compile import compile;compile('$(DSTROOT)$(CGIPY)', dfile='$(CGIPY)', doraise=True)"

##---------------------------------------------------------------------
# config/Makefile needs the following changes:
# remove -arch xxx flags
# 4144521 - correct LINKFORSHARED
# 3488297 - point BINDIR to /usr/local/bin
##---------------------------------------------------------------------
INSTALLPY = $(LIBPYTHONVERS)/distutils/command/install.py
fix-config-Makefile:
	ed - '$(DSTROOT)$(LIBPYTHONVERS)/config/Makefile' < $(FIX)/config_Makefile.ed

fix-usr-bin:
	@set -x && \
	cd '$(DSTROOT)$(USRBIN)' && \
	rm -f idle* && \
	for i in *; do \
	    rm -f $$i && \
	    ln -s ../..$(VERSIONSVERS)/bin/$$i || exit 1; \
	done

LIBRARYPYTHON = /Library/Python
LIBRARYPYTHONVERS = $(LIBRARYPYTHON)/$(VERS)

fix-permissions:
ifeq ($(shell id -u), 0)
	@set -x && \
	for i in Applications Developer Library; do \
	    chgrp -Rf admin $(DSTROOT)/$$i && \
	    chmod -Rf g+w $(DSTROOT)/$$i; \
	done
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) '$(OSV)'
	$(INSTALL_FILE) '$(SRCROOT)/$(Project).plist' '$(OSV)/$(Project).plist'
	$(MKDIR) '$(OSL)'
	$(INSTALL_FILE) '$(OBJROOT)/LICENSE' '$(OSL)/$(Project).txt'
