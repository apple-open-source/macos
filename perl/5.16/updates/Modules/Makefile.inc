# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION := $(NAME)-$(VERSION)
TARBALL := $(NAMEVERSION).tar.gz
DIR := $(OBJROOT)/$(NAMEVERSION)
MYSRC := $(SRCROOT)/Modules/$(NAMEVERSION)

ifeq ($(shell [ -f $(DIR)/Build.PL ] && echo YES || echo NO),YES)
PLFILE = Build.PL
MAKECMD = perl ./Build
INDIRECTPLARGS = PLBARGS
INDIRECTMAKEARGS = BUILDARGS
else
PLFILE = Makefile.PL
MAKECMD = make
INDIRECTPLARGS = PLARGS
INDIRECTMAKEARGS = MAKEARGS
endif

no_target: $(DIR)/.mark

$(DIR)/.mark: $(DIR) configure build
	touch $@

configure: custompatching
	@set -x && \
	cd $(DIR) && \
	PERL_MM_USE_DEFAULT=1 ARCHFLAGS='$(ARCHFLAGS)' $(ENV) perl $(PLFILE) $(PROJARGS) $($(INDIRECTPLARGS))

build: patchafterconfigure
	@set -x && \
	cd $(DIR) && \
	ARCHFLAGS='$(ARCHFLAGS)' $(ENV) $(MAKECMD) $($(INDIRECTMAKEARGS))

$(DIR):
	gnutar xozf $(TARBALL) -C $(OBJROOT)

unpack: $(DIR)

install: realinstall postinstall

realinstall: $(DIR)/.mark
	@set -x && \
	cd $(DIR) && \
	ARCHFLAGS='$(ARCHFLAGS)' $(ENV) $(MAKECMD) install $($(INDIRECTMAKEARGS))

# allow custom patching before running Makefile.PL or Build.PL
custompatching:

# allow patching after running Makefile.PL or Build.PL
patchafterconfigure:

# allow post install customization
postinstall:

# whether to avoid 64-bit compiles
no64:
ifeq ($(NO64),YES)
	@echo YES
else
	@echo NO
endif
