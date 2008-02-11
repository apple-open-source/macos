##---------------------------------------------------------------------
# GNUmakefile for python
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = python
VERSION = 2.5.1

RSYNC = rsync -rlpt
PWD := $(shell pwd)
ifndef DSTROOT
ifdef DESTDIR
export DSTROOT := $(shell mkdir -p '$(DESTDIR)' && echo '$(DESTDIR)')
else
export DSTROOT := /
endif
endif
ifndef OBJROOT
export OBJROOT := $(shell mkdir -p '$(PWD)/OBJROOT' && echo '$(PWD)/OBJROOT')
RSYNC += --exclude=OBJROOT
endif
ifndef SRCROOT
export SRCROOT := $(PWD)
endif
ifndef SYMROOT
export SYMROOT := $(shell mkdir -p '$(PWD)/SYMROOT' && echo '$(PWD)/SYMROOT')
RSYNC += --exclude=SYMROOT
endif
ifndef RC_ARCHS
export RC_ARCHS := $(shell arch)
export RC_$(RC_ARCHS) := YES
endif
ifndef RC_CFLAGS
export RC_CFLAGS := $(foreach A,$(RC_ARCHS),-arch $(A)) $(RC_NONARCH_CFLAGS)
endif
ifndef RC_NONARCH_CFLAGS
export RC_NONARCH_CFLAGS = -pipe
endif
ifndef RC_ProjectName
export RC_ProjectName = $(PROJECT)
endif

NAME = Python
export PYTHON_CURRENT_VERSION = $(VERSION)
NAMEVERS = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERS).tar.bz2
FIX = '$(OBJROOT)/fix'

VERS := $(shell echo $(VERSION) | sed 's/\(^[0-9]*\.[0-9]*\).*/\1/')
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVERS = $(PYFRAMEWORK)/Versions/$(VERS)
INCLUDEPYTHONVERS = $(VERSIONSVERS)/include/$(PROJECT)$(VERS)
LIBPYTHONVERS = $(VERSIONSVERS)/lib/$(PROJECT)$(VERS)

MAJORVERS := $(shell echo $(VERS) | sed 's/\..*//')
DYLIB = lib$(PROJECT)$(MAJORVERS).dylib
USRINCLUDE = /usr/include
USRLIB = /usr/lib
EXTRAS = $(VERSIONSVERS)/Extras
LIBRARYPYTHON = /Library/Python/$(VERS)
SITEPACKAGES = $(LIBRARYPYTHON)/site-packages

EXTRASOBJROOT = '$(OBJROOT)/Extras-objroot'
EXTRASPYTHON = '$(DSTROOT)$(EXTRAS)/lib/python'
PYTHON = '$(OBJROOT)/$(PROJECT)/$(PROJECT).exe'
PYTHONENV = DYLD_FRAMEWORK_PATH='$(DSTROOT)$(FRAMEWORKS)' DYLD_NEW_LOCAL_SHARED_REGIONS=1 PYTHONPATH='$(DSTROOT)$(LIBPYTHONVERS)':$(EXTRASPYTHON)
PYDTRACE_H = $(OBJROOT)/$(PROJECT)/Python/pydtrace.h

# This file, along with the "strip" perl script, works around a verification
# error caused by a UFS bug (stripping a multi-link file breaks the link, and
# sometimes causes the wrong file to be stripped/unstripped).  By using the
# "strip" perl script, it not only causes the correct file to be stripped, but
# also preserves the link.

export PATH:=$(SRCROOT)/bin:$(PATH)

no_target: extras

python: $(OBJROOT)/$(PROJECT)
	DSTROOT='$(DSTROOT)' OBJROOT='$(OBJROOT)/$(PROJECT)' \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    DYLD_NEW_LOCAL_SHARED_REGIONS=1 \
	    $(MAKE) -C '$(OBJROOT)' -f Makefile \
	    DSTROOT='$(DSTROOT)' OBJROOT='$(OBJROOT)/$(PROJECT)' \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    VERS=$(VERS)

extras: python
	mkdir -p $(EXTRASOBJROOT)
	DSTROOT='$(DSTROOT)' OBJROOT=$(EXTRASOBJROOT) \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    $(MAKE) -C Extras EXTRAS='$(DSTROOT)$(EXTRAS)' \
	    EXTRASPYTHON=$(EXTRASPYTHON) LIBPYTHON='$(DSTROOT)$(LIBPYTHONVERS)' \
	    PYTHON=$(PYTHON) PYTHONENV="$(PYTHONENV)" \
	    LIBRARYPYTHON='$(DSTROOT)$(LIBRARYPYTHON)' \
	    DSTROOT='$(DSTROOT)' OBJROOT=$(EXTRASOBJROOT) \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'

##---------------------------------------------------------------------
# We patch configure to remove the "-arch_only ppc" option, since we
# build fat.  We also set DYLD_NEW_LOCAL_SHARED_REGIONS or else python.exe
# will crash.  And patch unixccompiler for wxWidgets (submit back to python).
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	$(RSYNC) '$(SRCROOT)/' '$(OBJROOT)'
	@set -x && \
	cd '$(OBJROOT)' && \
	bzcat $(TARBALL) | gnutar xf - && \
	rm -rf $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	ed - $(PROJECT)/configure.in < $(FIX)/configure.in.ed && \
	ed - $(PROJECT)/Include/pymactoolbox.h < $(FIX)/pymactoolbox.h.ed && \
	ed - $(PROJECT)/Lib/ctypes/__init__.py < $(FIX)/Lib_ctypes___init__.py.ed && \
	ed - $(PROJECT)/Lib/xml/__init__.py < $(FIX)/Lib_xml___init__.py.ed && \
	ed - $(PROJECT)/Lib/distutils/command/install.py < $(FIX)/install.py.ed && \
	ed - $(PROJECT)/Lib/distutils/sysconfig.py < $(FIX)/sysconfig.py.ed && \
	ed - $(PROJECT)/Lib/distutils/unixccompiler.py < $(FIX)/unixccompiler.py.ed && \
	ed - $(PROJECT)/Lib/locale.py < $(FIX)/locale.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/bundlebuilder.py < $(FIX)/bundlebuilder.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/Carbon/AppleEvents.py < $(FIX)/AppleEvents.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/terminalcommand.py < $(FIX)/terminalcommand.py.ed && \
	ed - $(PROJECT)/Lib/pydoc.py < $(FIX)/pydoc.py.ed && \
	ed - $(PROJECT)/Lib/site.py < $(FIX)/site.py.ed && \
	ed - $(PROJECT)/Mac/Makefile.in  < $(FIX)/Mac_Makefile.in.ed && \
	ed - $(PROJECT)/Mac/Modules/cg/_CGmodule.c  < $(FIX)/_CGmodule.c.ed && \
	ed - $(PROJECT)/Mac/Modules/launch/_Launchmodule.c < $(FIX)/_Launchmodule.c.ed && \
	ed - $(PROJECT)/Makefile.pre.in < $(FIX)/Makefile.pre.in.ed && \
	patch $(PROJECT)/Misc/python.man $(FIX)/python.man.patch && \
	ed - $(PROJECT)/Modules/dlmodule.c < $(FIX)/dlmodule.c.ed && \
	ed - $(PROJECT)/Modules/getpath.c < $(FIX)/getpath.c.ed && \
	ed - $(PROJECT)/Modules/_localemodule.c < $(FIX)/_localemodule.c.ed && \
	ed - $(PROJECT)/Modules/readline.c < $(FIX)/readline.c.ed && \
	ed - $(PROJECT)/pyconfig.h.in < $(FIX)/pyconfig.h.in.ed && \
	ed - $(PROJECT)/Python/ast.c < $(FIX)/ast.c.ed && \
	ed - $(PROJECT)/Python/ceval.c < $(FIX)/ceval.c.ed && \
	ed - $(PROJECT)/Python/mactoolboxglue.c < $(FIX)/mactoolboxglue.c.ed && \
	ed - $(PROJECT)/setup.py < $(FIX)/setup.py.ed && \
	for i in `find $(PROJECT)/Mac -name setup.py | xargs fgrep -l "'-framework', 'Carbon'"`; do \
	    ed - $$i < $(FIX)/Mac_setup.py.ed || exit 1; \
	done && \
	for i in `find $(PROJECT)/Lib -name __init__.py -size 0c`; do \
	    echo '#' > $$i; \
	done
	cd '$(OBJROOT)/$(PROJECT)' && patch -p1 -i $(FIX)/CVE-2007-4965-int-overflow.patch
	cd '$(OBJROOT)/$(PROJECT)' && autoconf
	dtrace -h -s $(FIX)/pydtrace.d -o '$(PYDTRACE_H)'

install: installpython installextras
	install $(FIX)/audiotest.au $(DSTROOT)$(LIBPYTHONVERS)/email/test/data/audiotest.au
	install $(FIX)/audiotest.au $(DSTROOT)$(LIBPYTHONVERS)/test/audiotest.au

##---------------------------------------------------------------------
# PR-3478215 - for backwards compatibility with non-framework python, we
# create symbolic links in /usr/include and /usr/lib.
##---------------------------------------------------------------------
installpython: $(OBJROOT)/$(PROJECT)
	install -d '$(DSTROOT)/usr/bin'
	DSTROOT='$(DSTROOT)' OBJROOT='$(OBJROOT)/$(PROJECT)' \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    DYLD_NEW_LOCAL_SHARED_REGIONS=1 \
	    $(MAKE) -C '$(OBJROOT)' -f Makefile install \
	    DSTROOT='$(DSTROOT)' OBJROOT='$(OBJROOT)/$(PROJECT)' \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    VERS=$(VERS)
	@obj= && \
	install -d '$(DSTROOT)$(USRINCLUDE)'
	ln -sf ../..$(INCLUDEPYTHONVERS) '$(DSTROOT)$(USRINCLUDE)/$(PROJECT)$(VERS)'
	install -d '$(DSTROOT)$(USRLIB)'
	ln -sf ../..$(LIBPYTHONVERS) '$(DSTROOT)$(USRLIB)/$(PROJECT)$(VERS)'
	ln -sf ../..$(VERSIONSVERS)/Python '$(DSTROOT)$(USRLIB)/$(DYLIB)'
	ln -sf $(DYLIB) '$(DSTROOT)$(USRLIB)/lib$(PROJECT)$(VERS).dylib'
	ln -sf $(DYLIB) '$(DSTROOT)$(USRLIB)/lib$(PROJECT).dylib'
	install -d '$(DSTROOT)$(SITEPACKAGES)'

installextras:
	mkdir -p $(EXTRASOBJROOT)
	DSTROOT='$(DSTROOT)' OBJROOT=$(EXTRASOBJROOT) \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)' \
	    $(MAKE) -C Extras install EXTRAS='$(DSTROOT)$(EXTRAS)' \
	    EXTRASPYTHON=$(EXTRASPYTHON) LIBPYTHON='$(DSTROOT)$(LIBPYTHONVERS)' \
	    PYTHON=$(PYTHON) PYTHONENV="$(PYTHONENV)" \
	    LIBRARYPYTHON='$(DSTROOT)$(LIBRARYPYTHON)' \
	    DSTROOT='$(DSTROOT)' OBJROOT=$(EXTRASOBJROOT) \
	    SRCROOT='$(OBJROOT)' SYMROOT='$(SYMROOT)' RC_ARCHS='$(RC_ARCHS)'
	@set -x && \
	if [ -d $(EXTRASPYTHON) ]; then \
	    find $(EXTRASPYTHON) -name \*.so -exec strip -x {} \; && \
	    for i in `find $(EXTRASPYTHON) -name __init__.py -size 0c`; do \
		echo '#' > $$i && \
		j=`echo "$$i" | sed 's,^$(DSTROOT),,'` && \
		$(PYTHONENV) $(PYTHON) -c "from py_compile import compile;compile('$$i', dfile='$$j', doraise=True)" && \
		$(PYTHONENV) $(PYTHON) -O -c "from py_compile import compile;compile('$$i', dfile='$$j', doraise=True)"; \
	    done; \
	fi

.DEFAULT:
	@$(MAKE) -f Makefile $@
