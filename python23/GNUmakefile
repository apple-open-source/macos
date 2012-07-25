##---------------------------------------------------------------------
# GNUmakefile for python
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = python
NAME = Python
VERSION = 2.3.5
export PYTHON_CURRENT_VERSION = $(VERSION)
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.bz2
FIX = $(OBJROOT)/fix

GCC_VERSION := $(shell cc -dumpversion | sed -e 's/^\([^.]*\.[^.]*\).*/\1/')
GCC_42 := $(shell perl -e "print ($(GCC_VERSION) >= 4.2 ? 'YES' : 'NO')")

VERS = 2.3
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVERS = $(PYFRAMEWORK)/Versions/$(VERS)
INCLUDEPYTHONVERS = $(VERSIONSVERS)/include/$(PROJECT)$(VERS)
LIBPYTHONVERS = $(VERSIONSVERS)/lib/$(PROJECT)$(VERS)

MAJORVERS = $(shell echo $(VERS) | sed 's/\..*//')
DYLIB = lib$(PROJECT)$(MAJORVERS).dylib
USRINCLUDE = /usr/include
USRLIB = /usr/lib
EXTRAS = $(VERSIONSVERS)/Extras
LIBRARYPYTHON = /Library/Python/$(VERS)
SITEPACKAGES = $(LIBRARYPYTHON)/site-packages

PYTHONENV = DYLD_FRAMEWORK_PATH=$(DSTROOT)$(FRAMEWORKS) DYLD_NEW_LOCAL_SHARED_REGIONS=1 PYTHONPATH="$(DSTROOT)$(LIBPYTHONVERS):$(EXTRASPYTHON)"

# This file, along with the "strip" perl script, works around a verification
# error caused by a UFS bug (stripping a multi-link file breaks the link, and
# sometimes causes the wrong file to be stripped/unstripped).  By using the
# "strip" perl script, it not only causes the correct file to be stripped, but
# also preserves the link.

export PATH:=$(SRCROOT)/bin:$(PATH)

no_target: python

python: $(OBJROOT)/$(PROJECT)
	DYLD_NEW_LOCAL_SHARED_REGIONS=1 $(MAKE) -C $(OBJROOT) -f Makefile \
		SRCROOT=$(OBJROOT) OBJROOT="$(OBJROOT)/$(PROJECT)" \
		VERS=$(VERS) GCC_42=$(GCC_42)

##---------------------------------------------------------------------
# We patch configure to remove the "-arch_only ppc" option, since we
# build fat.  We also set DYLD_NEW_LOCAL_SHARED_REGIONS or else python.exe
# will crash.  And patch unixccompiler for wxWidgets (submit back to python).
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	rsync -a $(SRCROOT)/ $(OBJROOT)
	@set -x && \
	cd $(OBJROOT) && \
	bzcat $(TARBALL) | gnutar xf - && \
	rm -rf $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	ed - $(PROJECT)/configure < $(FIX)/configure.ed && \
	ed - $(PROJECT)/Makefile.pre.in < $(FIX)/Makefile.pre.in.ed && \
	ed - $(PROJECT)/Lib/distutils/unixccompiler.py < $(FIX)/unixccompiler.py.ed && \
	ed - $(PROJECT)/Lib/locale.py < $(FIX)/locale.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/Carbon/AppleEvents.py < $(FIX)/AppleEvents.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/terminalcommand.py < $(FIX)/terminalcommand.py.ed && \
	ed - $(PROJECT)/Modules/getpath.c < $(FIX)/getpath.c.ed && \
	ed - $(PROJECT)/Modules/_localemodule.c < $(FIX)/_localemodule.c.ed && \
	ed - $(PROJECT)/pyconfig.h.in < $(FIX)/pyconfig.h.in.ed && \
	ed - $(PROJECT)/Python/mactoolboxglue.c < $(FIX)/mactoolboxglue.c.ed
	cd '$(OBJROOT)/$(PROJECT)' && patch -p1 -i $(FIX)/CVE-2007-4965-int-overflow.patch
ifeq "$(GCC_42)" "YES"
	@set -x && \
	cd $(OBJROOT) && \
	ed - $(PROJECT)/configure < $(FIX)/configure42.ed
endif

install: installpython
	install $(FIX)/audiotest.au $(DSTROOT)$(LIBPYTHONVERS)/email/test/data/audiotest.au
	install $(FIX)/audiotest.au $(DSTROOT)$(LIBPYTHONVERS)/test/audiotest.au

installpython: $(OBJROOT)/$(PROJECT)
	DYLD_NEW_LOCAL_SHARED_REGIONS=1 $(MAKE) -C $(OBJROOT) -f Makefile \
		install SRCROOT=$(OBJROOT) OBJROOT="$(OBJROOT)/$(PROJECT)" \
		VERS=$(VERS) GCC_42=$(GCC_42)
	#ln -sf $(DYLIB) $(DSTROOT)$(USRLIB)/lib$(PROJECT)$(VERS).dylib
	#ln -sf $(DYLIB) $(DSTROOT)$(USRLIB)/lib$(PROJECT).dylib
	install -d $(DSTROOT)$(SITEPACKAGES)
	echo $(EXTRAS)/lib/python > $(DSTROOT)$(SITEPACKAGES)/Extras.pth

.DEFAULT:
	@$(MAKE) -f Makefile $@ GCC_42=$(GCC_42)
