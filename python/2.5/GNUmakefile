##---------------------------------------------------------------------
# GNUmakefile for python
# Call Makefile to do the work, but for the install case, unpack the
# tarball to create the project source directory
##---------------------------------------------------------------------
PROJECT = python
VERSION = 2.5.6

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
endif
ifndef SRCROOT
export SRCROOT := $(PWD)
endif
ifndef SYMROOT
export SYMROOT := $(shell mkdir -p '$(PWD)/SYMROOT' && echo '$(PWD)/SYMROOT')
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
# The default ARCHFLAGS is the union of RC_ARCHS and tconf --archs (without
# RC_ARCHS set); we also remove the 64-bit architectures.
##---------------------------------------------------------------------
archs = $(shell export -n RC_ARCHS && tconf --archs)
union = $(foreach a,$(archs),$(findstring $(a),$(RC_ARCHS)))
defarchflags = $(foreach u,$(filter-out %64,$(union)),-arch $(u))

$(OBJROOT)/$(PROJECT):
	@set -x && \
	cd '$(OBJROOT)' && \
	bzcat $(TARBALL) | gnutar xof - && \
	rm -rf $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	ed - $(PROJECT)/configure.in < $(FIX)/configure.in.ed && \
	ed - $(PROJECT)/Include/py_curses.h < $(FIX)/py_curses.h.ed && \
	ed - $(PROJECT)/Include/pymactoolbox.h < $(FIX)/pymactoolbox.h.ed && \
	ed - $(PROJECT)/Lib/ctypes/__init__.py < $(FIX)/Lib_ctypes___init__.py.ed && \
	ed - $(PROJECT)/Lib/xml/__init__.py < $(FIX)/Lib_xml___init__.py.ed && \
	ed - $(PROJECT)/Lib/distutils/command/install.py < $(FIX)/install.py.ed && \
	sed 's/@DEFARCHFLAGS@/$(defarchflags)/' $(FIX)/sysconfig.py.ed | ed - $(PROJECT)/Lib/distutils/sysconfig.py && \
	ed - $(PROJECT)/Lib/distutils/unixccompiler.py < $(FIX)/unixccompiler.py.ed && \
	ed - $(PROJECT)/Lib/locale.py < $(FIX)/locale.py.ed && \
	ed - $(PROJECT)/Lib/logging/handlers.py < $(FIX)/Lib_logging_handlers.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/bundlebuilder.py < $(FIX)/bundlebuilder.py.ed && \
	ed - $(PROJECT)/Lib/plat-mac/Carbon/AppleEvents.py < $(FIX)/AppleEvents.py.ed && \
	ed - $(PROJECT)/Lib/pydoc.py < $(FIX)/pydoc.py.ed && \
	ed - $(PROJECT)/Lib/rlcompleter.py < $(FIX)/rlcompleter.py.ed && \
	ed - $(PROJECT)/Lib/site.py < $(FIX)/site.py.ed && \
	ed - $(PROJECT)/Lib/test/test__locale.py < $(FIX)/Lib_test_test__locale.py.ed && \
	ed - $(PROJECT)/Mac/Makefile.in  < $(FIX)/Mac_Makefile.in.ed && \
	patch $(PROJECT)/Mac/Modules/cf/_CFmodule.c $(FIX)/_CFmodule.c.patch && \
	ed - $(PROJECT)/Mac/Modules/cg/_CGmodule.c  < $(FIX)/_CGmodule.c.ed && \
	ed - $(PROJECT)/Mac/Modules/launch/_Launchmodule.c < $(FIX)/_Launchmodule.c.ed && \
	ed - $(PROJECT)/Mac/Tools/pythonw.c < $(FIX)/pythonw.c.ed && \
	ed - $(PROJECT)/Makefile.pre.in < $(FIX)/Makefile.pre.in.ed && \
	patch $(PROJECT)/Misc/python.man $(FIX)/python.man.patch && \
	ed - $(PROJECT)/Modules/_cursesmodule.c < $(FIX)/_cursesmodule.c.ed && \
	ed - $(PROJECT)/Modules/dlmodule.c < $(FIX)/dlmodule.c.ed && \
	rm $(PROJECT)/Modules/expat/expat.h $(PROJECT)/Modules/expat/expat_external.h && \
	ed - $(PROJECT)/Modules/getpath.c < $(FIX)/getpath.c.ed && \
	ed - $(PROJECT)/Modules/_localemodule.c < $(FIX)/_localemodule.c.ed && \
	ed - $(PROJECT)/Modules/posixmodule.c < $(FIX)/posixmodule.c.ed && \
	sed 's/@VERSION@/$(VERS)/' $(FIX)/python.c.ed | ed - $(PROJECT)/Modules/python.c && \
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
	@set -x && \
	cd '$(OBJROOT)/$(PROJECT)' && \
	patch -p0 -i $(FIX)/qt-changes.diff && \
	patch -p0 -i $(FIX)/CVE-2009-4134,CVE-2010-1449,CVE-2010-1450-rgbimg-int-overflow.patch && \
	patch -p0 -i $(FIX)/additional-2.6.7-security-fixes.patch
	@set -x && \
	cd '$(OBJROOT)' && \
	ed - $(PROJECT)/configure.in < $(FIX)/configure42.in.ed
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
