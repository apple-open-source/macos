# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION := $(NAME)-$(VERSION)
TARBALL := $(NAMEVERSION).tar.gz
DIR := $(OBJROOT)/$(NAMEVERSION)
MYSRC := $(SRCROOT)/Modules/$(NAMEVERSION)
ARCHFLAGS := ARCHFLAGS='$(RC_CFLAGS)'
export PATH := $(DSTROOT)/usr/bin:$(PATH)

no_target: $(DIR)/.mark

$(DIR)/.mark: $(DIR) custompatching
	@set -x && \
	cd $(DIR) && \
	PERL_MM_USE_DEFAULT=1 $(ARCHFLAGS) $(ENV) perl Makefile.PL $(PROJARGS) $(PLARGS) && \
	$(ARCHFLAGS) $(ENV) make $(MAKEARGS)
	touch $@

$(DIR):
	gnutar xozf $(TARBALL) -C $(OBJROOT)

install: realinstall postinstall

realinstall: $(DIR)/.mark
	@set -x && \
	cd $(DIR) && \
	$(ARCHFLAGS) $(ENV) make install $(MAKEARGS)

# allow custom patching before running Makefile.PL
custompatching:

# allow post install customization
postinstall:

# whether to avoid 64-bit compiles
no64:
ifeq ($(NO64),YES)
	@echo YES
else
	@echo NO
endif
