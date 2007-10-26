# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION := $(NAME)-$(VERSION)
TARBALL := $(NAMEVERSION).tar.gz
DIR := $(OBJROOT)/$(NAME)/$(NAMEVERSION)
LD := $(shell perl -MConfig -e 'print $$Config::Config{ld}') $(RC_CFLAGS)
FLAGS := PASTHRU_INC='$(RC_CFLAGS)' LD='$(LD)'
export PATH := $(DSTROOT)/usr/bin:$(PATH)

no_target: $(DIR)/.mark

$(DIR)/.mark: $(DIR) custompatching
	@set -x && \
	cd $(DIR) && \
	PERL_MM_USE_DEFAULT=1 $(ENV) perl Makefile.PL $(PROJARGS) $(PLARGS) && \
	$(ENV) make $(MAKEARGS) $(FLAGS)
	touch $@

$(DIR):
	mkdir -p $(OBJROOT)/$(NAME)
	gnutar xzf $(TARBALL) -C $(OBJROOT)/$(NAME)

install: realinstall postinstall

realinstall: $(DIR)/.mark
	@set -x && \
	cd $(DIR) && \
	$(ENV) make install $(MAKEARGS) $(FLAGS)

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
