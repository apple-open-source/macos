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

VERS = 2.3
FRAMEWORKS = /System/Library/Frameworks
PYFRAMEWORK = $(FRAMEWORKS)/Python.framework
VERSIONSVERS = $(PYFRAMEWORK)/Versions/$(VERS)
INCLUDEPYTHONVERS = $(VERSIONSVERS)/include/$(PROJECT)$(VERS)
LIBPYTHONVERS = $(VERSIONSVERS)/lib/$(PROJECT)$(VERS)

MAJORVERS = $(shell echo $(VERS) | sed 's/\..*//')
DYLIB = lib$(PROJECT)$(MAJORVERS).dylib
MAN1 = /usr/share/man/man1
USRINCLUDE = /usr/include
USRLIB = /usr/lib
EXTRAS = $(VERSIONSVERS)/Extras
LIBRARYPYTHON = /Library/Python/$(VERS)
SITEPACKAGES = $(LIBRARYPYTHON)/site-packages

EXTRASOBJROOT=$(OBJROOT)/Extras-objroot
EXTRASPYTHON = $(DSTROOT)$(EXTRAS)/lib/python
PYTHON = $(DSTROOT)/usr/bin/python
PYTHONENV = DYLD_FRAMEWORK_PATH=$(DSTROOT)$(FRAMEWORKS) DYLD_NEW_LOCAL_SHARED_REGIONS=1 PYTHONPATH="$(DSTROOT)$(LIBPYTHONVERS):$(EXTRASPYTHON)"

no_target: extras

python: $(OBJROOT)/$(PROJECT)
	DYLD_NEW_LOCAL_SHARED_REGIONS=1 $(MAKE) -C $(OBJROOT) -f Makefile \
		SRCROOT=$(OBJROOT) OBJROOT="$(OBJROOT)/$(PROJECT)" \
		VERS=$(VERS)

extras: python
	mkdir -p $(EXTRASOBJROOT)
	$(MAKE) -C Extras EXTRAS=$(DSTROOT)$(EXTRAS) \
	    EXTRASPYTHON=$(EXTRASPYTHON)  LIBPYTHON=$(DSTROOT)$(LIBPYTHONVERS) \
	    PYTHON=$(PYTHON) PYTHONENV="$(PYTHONENV)" OBJROOT=$(EXTRASOBJROOT) \
	    LIBRARYPYTHON=$(DSTROOT)$(LIBRARYPYTHON)

##---------------------------------------------------------------------
# We patch configure to remove the "-arch_only ppc" option, since we
# build fat.  We also set DYLD_NEW_LOCAL_SHARED_REGIONS or else python.exe
# will crash.  And patch unixccompiler for wxWidgets (submit back to python).
##---------------------------------------------------------------------
$(OBJROOT)/$(PROJECT):
	rsync -a $(SRCROOT)/ $(OBJROOT)
	@echo cd $(OBJROOT) && \
	cd $(OBJROOT) && \
	echo bzcat $(TARBALL) \| gnutar xf - && \
	bzcat $(TARBALL) | gnutar xf - && \
	echo rm -rf $(PROJECT) && \
	rm -rf $(PROJECT) && \
	echo mv $(NAMEVERS) $(PROJECT) && \
	mv $(NAMEVERS) $(PROJECT) && \
	echo ed - $(PROJECT)/configure \< $(FIX)/configure.ed && \
	ed - $(PROJECT)/configure < $(FIX)/configure.ed && \
	echo ed - $(PROJECT)/Lib/distutils/unixccompiler.py \< $(FIX)/unixccompiler.py.ed && \
	ed - $(PROJECT)/Lib/distutils/unixccompiler.py < $(FIX)/unixccompiler.py.ed && \
	echo ed - $(PROJECT)/Lib/locale.py \< $(FIX)/locale.py.ed && \
	ed - $(PROJECT)/Lib/locale.py < $(FIX)/locale.py.ed && \
	echo ed - $(PROJECT)/Modules/_localemodule.c \< $(FIX)/_localemodule.c.ed && \
	ed - $(PROJECT)/Modules/_localemodule.c < $(FIX)/_localemodule.c.ed

install: installpython installextras

##---------------------------------------------------------------------
# PR-3478215 - for backwards compatibility with non-framework python, we
# create symbolic links in /usr/include and /usr/lib.
##---------------------------------------------------------------------
installpython: $(OBJROOT)/$(PROJECT)
	DYLD_NEW_LOCAL_SHARED_REGIONS=1 $(MAKE) -C $(OBJROOT) -f Makefile \
		install SRCROOT=$(OBJROOT) OBJROOT="$(OBJROOT)/$(PROJECT)" \
		VERS=$(VERS)
	@obj= && \
	install -d $(DSTROOT)$(USRINCLUDE)
	ln -sf ../..$(INCLUDEPYTHONVERS) $(DSTROOT)$(USRINCLUDE)/$(PROJECT)$(VERS)
	install -d $(DSTROOT)$(USRLIB)
	ln -sf ../..$(LIBPYTHONVERS) $(DSTROOT)$(USRLIB)/$(PROJECT)$(VERS)
	ln -sf ../..$(VERSIONSVERS)/Python $(DSTROOT)$(USRLIB)/$(DYLIB)
	ln -sf $(DYLIB) $(DSTROOT)$(USRLIB)/lib$(PROJECT)$(VERS).dylib
	ln -sf $(DYLIB) $(DSTROOT)$(USRLIB)/lib$(PROJECT).dylib
	install -d $(DSTROOT)$(SITEPACKAGES)
	echo $(EXTRAS)/lib/python > $(DSTROOT)$(SITEPACKAGES)/Extras.pth
	install -m 0644 $(FIX)/pydoc.1 $(DSTROOT)$(MAN1)
	install -m 0644 $(FIX)/pythonw.1 $(DSTROOT)$(MAN1)
	ln -sf python.1 $(DSTROOT)$(MAN1)/python$(VERS).1
	ln -sf pythonw.1 $(DSTROOT)$(MAN1)/pythonw$(VERS).1

installextras:
	mkdir -p $(EXTRASOBJROOT)
	$(MAKE) -C Extras install EXTRAS=$(DSTROOT)$(EXTRAS) \
	    EXTRASPYTHON=$(EXTRASPYTHON)  LIBPYTHON=$(DSTROOT)$(LIBPYTHONVERS) \
	    PYTHON=$(PYTHON) PYTHONENV="$(PYTHONENV)" OBJROOT=$(EXTRASOBJROOT) \
	    LIBRARYPYTHON=$(DSTROOT)$(LIBRARYPYTHON)
	@if [ -d $(EXTRASPYTHON) ]; then \
	    echo find $(EXTRASPYTHON) -name \*.so -exec strip -x {} \; && \
	    find $(EXTRASPYTHON) -name \*.so -exec strip -x {} \; && \
	    for i in `find $(EXTRASPYTHON) -name __init__.py -size 0c`; do \
		echo echo '#' \> $$i && \
		echo '#' > $$i && \
		echo touch $${i}c && \
		touch $${i}c; \
	    done; \
	fi

.DEFAULT:
	@$(MAKE) -f Makefile $@
