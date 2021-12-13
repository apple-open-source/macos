# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION := $(NAME)-$(VERSION)
TARBALL := $(NAMEVERSION).tar.gz
DIR := $(OBJROOT)/$(NAMEVERSION)

INCLUDEPYTHONVERS = $(PYPREFIX)/Headers
export PYFRAMEWORKNAME = $(shell echo $(PYPREFIX) | cut -d '/' -f5)
export PYAPPNAME = $(shell echo '$(PYFRAMEWORKNAME)' | sed 's,\.framework,,')
export PYEXECUTABLE = $(PYPREFIX)/Resources/Python.app/Contents/MacOS/$(PYAPPNAME)

ifeq ($(TOPDIR),AppleInternal)
export LIBRARYPREFIX = /AppleInternal/Library
export __INTERNALSDKBUILD__ = YES
else
export LIBRARYPREFIX = /Library
endif

$(DIR):
	gnutar xozf $(TARBALL) -C $(OBJROOT)

afterinstallsrc::
	@echo "# $(NAMEVERSION): afterinstallsrc"

install: realinstall postinstall

realinstall: $(DIR) patchall custompatching
	cd $(DIR) && \
	ARCHFLAGS="$(RC_CFLAGS) -iwithsysroot /usr/local/libressl-3.3/include -L$(SDKROOT)/usr/local/libressl-3.3/lib" \
	python setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)" $(SETUPARGS)

# allow custom patching before running setup.py
custompatching:

# install custom patch on all modules before running setup.py
patchall:
	sed -e 's,@SDKROOT@,$(SDKROOT),g' -e 's,@INCLUDEPYTHONVERS@,$(INCLUDEPYTHONVERS),g' $(SRCROOT)/Modules/setup.cfg.patch >> $(DIR)/setup.cfg

# allow post install customization
postinstall:

# whether to avoid 64-bit compiles
no64:
ifeq ($(NO64),YES)
	@echo YES
else
	@echo NO
endif
