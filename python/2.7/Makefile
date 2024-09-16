##
# Makefile for python
##
Allowable_Clients = $(shell cat $(SRCROOT)/allowableclients.txt)
Allowable_Clients_Flags = $(addprefix -allowable_client ,$(Allowable_Clients))

ifeq ($(INTERNALINSTALL), YES) 
$(info Installing 2.7 for internal users only)
Install_Prefix = /usr/local
Install_Man = /usr/local/share/man
Install_Info = /usr/local/share/info
Install_HTML = /usr/local/share/html
else
$(info Installing 2.7 for all users)
endif

USRBIN = $(USRPREFIX)/bin
FRAMEWORKS = $(INSTALLPREFIX)/Library/Frameworks
MAN1 = $(USRPREFIX)/share/man/man1

Project               = python
Extra_Configure_Flags = --enable-ipv6 --with-system-expat --with-threads --enable-framework=$(FRAMEWORKS) --enable-toolbox-glue --with-system-ffi --with-framework-name=$(PYFRAMEWORKNAME)
# Define the environment variable DEBUG to YES, to build without optimization
ifeq ($(DEBUG),YES)
Extra_Configure_Flags += --with-pydebug
endif

##---------------------------------------------------------------------
# Extra_CC_Flags and Extra_LD_Flags are needed because CFLAGS gets overridden
# by the RC_* variables.  These values would normally be set by the default
# python Makefile
##---------------------------------------------------------------------
Extra_CC_Flags += -fno-common -fno-strict-aliasing -fwrapv -DENABLE_DTRACE -DMACOSX -DNDEBUG -Wall -Wstrict-prototypes -Wshorten-64-to-32 -iwithsysroot /usr/local/libressl-3.3/include
Extra_LD_Flags += -Wl,-F. -L$(SDKROOT)/usr/local/libressl-3.3/lib $(Allowable_Clients_Flags)
Extra_Install_Flags   = DESTDIR='$(DSTROOT)'
GnuAfterInstall       = fixup-after-install install-plist
Extra_Environment     = CCSHARED='$(RC_CFLAGS)'

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Override compiler setting from GNUSource.make to use those set in GNUmakefile
override CC = $(MY_CC)
override CXX = $(MY_CXX)

Install_Flags := $(shell echo $(Install_Flags) | sed 's/prefix=[^ ]* *//')
Install_Target = frameworkinstall

FIX = '$(SRCROOT)/fix'

##---------------------------------------------------------------------
# Patch Makefiles and pyconfig.h just after running configure
##---------------------------------------------------------------------
ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	sed 's,@LIBRARYPREFIX@,$(LIBRARYPREFIX),' $(FIX)/Makefile.ed | ed - $(OBJROOT)/Makefile
	ed - '$(OBJROOT)/pyconfig.h' < $(FIX)/pyconfig.ed
	$(TOUCH) $(ConfigStamp2)

##---------------------------------------------------------------------
# Fixup a lot of problems after the install
##---------------------------------------------------------------------
APPS = /Applications
PYTHONAPPNAME = Python.app
PYFRAMEWORK = $(FRAMEWORKS)/$(PYFRAMEWORKNAME).framework
VERSIONSVERS = $(PYFRAMEWORK)/Versions/$(VERS)
RESOURCESVERS = $(VERSIONSVERS)/Resources
LIBPYTHONVERS = $(VERSIONSVERS)/lib/python$(VERS)
PYTHONAPP = $(RESOURCESVERS)/$(PYTHONAPPNAME)
PACONTENTS = $(PYTHONAPP)/Contents
PAMACOS = $(PACONTENTS)/MacOS
RUNPYTHON = DYLD_FRAMEWORK_PATH='$(OBJROOT)' '$(OBJROOT)/python.exe'
BYTE2UTF16 = $(RUNPYTHON) $(FIX)/byte2utf16.py
UTF162BYTE = $(RUNPYTHON) $(FIX)/utf162byte.py

fixup-after-install: delete-stuff \
		     strip-installed-files \
		     fix-PInfo \
		     fix-paths \
		     fix-usr-local-bin \
		     fix-usr-bin \
		     fix-permissions \
		     fix-config-Makefile \
		     additional-man-pages

# We leave "Build Applet.app" in $(APPS) to be deleted, because it uses
# Carbon, and doesn't work in 64-bit.  We also leave "Python Launcher.app"
# in $(APPS) to be deleted (10692664).
delete-stuff:
ifneq ($(INTERNALINSTALL), YES) 
	rm -rf '$(DSTROOT)$(APPS)'
	rm -rf '$(DSTROOT)/usr/local'
else
	rm -rf '$(DSTROOT)$(INSTALLPREFIX)$(APPS)'
endif
	# rdar://78720130 (Localization is requesting we remove the old and unused English.lproj)
	rm -rf '$(DSTROOT)$(RESOURCESVERS)/English.lproj'

# We now need to create the .dSYM bundles ourselves
# ($(DSTROOT)$(PYTHONAPP) is already stripped, so copy an unstripped binary)
strip-installed-files:
	ditto '$(DSTROOT)$(PYTHONAPP)' '$(SYMROOT)/$(PYTHONAPPNAME)'
	ditto '$(OBJROOT)/python.exe' '$(SYMROOT)/$(PYTHONAPPNAME)/Contents/MacOS/$(PYFRAMEWORKNAME)'
	$(DSYMUTIL) -o '$(SYMROOT)/$(PYTHONAPPNAME).dSYM' '$(SYMROOT)/$(PYTHONAPPNAME)/Contents/MacOS/$(PYFRAMEWORKNAME)'
	$(STRIP) -x '$(DSTROOT)$(PAMACOS)/$(PYFRAMEWORKNAME)'
	ditto '$(OBJROOT)/$(PYFRAMEWORKNAME).framework' '$(SYMROOT)/$(PYFRAMEWORKNAME).framework'
	$(DSYMUTIL) -o '$(SYMROOT)/$(PYFRAMEWORKNAME).framework.dSYM' '$(SYMROOT)/$(PYFRAMEWORKNAME).framework/$(PYFRAMEWORKNAME)'
	$(STRIP) -x '$(DSTROOT)$(VERSIONSVERS)/$(PYFRAMEWORKNAME)'
	ditto '$(DSTROOT)$(LIBPYTHONVERS)'/lib-dynload/*.so '$(SYMROOT)/lib-dynload/'
	cd '$(SYMROOT)/lib-dynload' && \
	for so in *.so; do \
	    $(DSYMUTIL) $$so || exit 1; \
	done
	$(STRIP) -x '$(DSTROOT)$(LIBPYTHONVERS)'/lib-dynload/*.so

fix-PInfo:
	ed - '$(DSTROOT)$(PACONTENTS)/Info.plist' < $(FIX)/pinfo.ed
	ed - '$(DSTROOT)$(RESOURCESVERS)/Info.plist' < $(FIX)/pinfo.ed
	patch '$(DSTROOT)$(PACONTENTS)/Info.plist' $(FIX)/Info.plist.identifier.patch
	patch '$(DSTROOT)$(RESOURCESVERS)/Info.plist' $(FIX)/Info.plist.identifier.patch

##---------------------------------------------------------------------
# As of python 2.7, the real man page is now python2.7.1 (not python.1).
##---------------------------------------------------------------------
additional-man-pages:
	mv -f $(DSTROOT)$(MAN1)/python$(VERS).1 $(DSTROOT)$(MAN1)/python.1
	install -m 0644 $(FIX)/pydoc.1 '$(DSTROOT)$(MAN1)'
	install -m 0644 $(FIX)/pythonw.1 '$(DSTROOT)$(MAN1)'
	ln -sf pydoc.1 '$(DSTROOT)$(MAN1)/pydoc$(VERS).1'
	ln -sf python.1 '$(DSTROOT)$(MAN1)/python$(VERS).1'
	ln -sf pythonw.1 '$(DSTROOT)$(MAN1)/pythonw$(VERS).1'

PYDOC = $(USRBIN)/pydoc
PYDOCORIG = $(VERSIONSVERS)/bin/pydoc

##---------------------------------------------------------------------
# The scripts exec $(VERSIONSVERS)/bin/python$(VERS), which execs
# $(PAMACOS)/Python.  We skip the extra exec by changing the scripts
# to exec $(PAMACOS)/Python directly.
##---------------------------------------------------------------------
fix-paths:
	@set -x && \
	cd '$(DSTROOT)$(VERSIONSVERS)/bin' && \
	for i in `find . -type f | sed 's,^\./,,'`; do \
	    if [ -n "`file $$i | fgrep script`" ]; then \
		sed 's,@PYFRAMEWORKNAME@,$(PYFRAMEWORKNAME),' $(FIX)/skipextraexec.ed | ed - $$i; \
	    fi || exit 1; \
	done

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
SYSCONFIGDATA = $(LIBPYTHONVERS)/_sysconfigdata.py
fix-config-Makefile:
	ed - '$(DSTROOT)$(LIBPYTHONVERS)/config/Makefile' < $(FIX)/config_Makefile.ed
	ed - '$(DSTROOT)$(SYSCONFIGDATA)' < $(FIX)/_sysconfigdata.py.ed$(PATCHEXT)
	$(RUNPYTHON) -c "from py_compile import compile;compile('$(DSTROOT)$(SYSCONFIGDATA)', dfile='$(SYSCONFIGDATA)', doraise=True)" && \
	$(RUNPYTHON) -O -c "from py_compile import compile;compile('$(DSTROOT)$(SYSCONFIGDATA)', dfile='$(SYSCONFIGDATA)', doraise=True)"

fix-usr-bin:
	@set -x && \
	cd '$(DSTROOT)$(USRBIN)' && \
	rm -f idle* && \
	for i in *; do \
	    rm -f $$i && \
	    ln -s $(BINBACKPATH)$(VERSIONSVERS)/bin/$$i || exit 1; \
	done

fix-permissions:
ifneq ($(INTERNALINSTALL), YES) 
ifeq ($(shell id -u), 0)
	@set -x && \
	for i in Applications Developer Library; do \
	    chgrp -Rf admin $(DSTROOT)/$$i && \
	    chmod -Rf g+w $(DSTROOT)/$$i; \
	done
endif
endif

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) '$(DSTROOT)$(OSV)'
	$(INSTALL_FILE) '$(SRCROOT)/$(Project).plist' '$(DSTROOT)$(OSV)/$(Project).plist'
	$(MKDIR) '$(DSTROOT)$(OSL)'
	$(INSTALL_FILE) '$(OBJROOT)/LICENSE' '$(DSTROOT)$(OSL)/$(Project).txt'
