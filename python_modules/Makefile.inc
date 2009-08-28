# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION := $(NAME)-$(VERSION)
TARBALL := $(NAMEVERSION).tar.gz
DIR := $(OBJROOT)/$(NAME)/$(NAMEVERSION)

$(DIR):
	mkdir -p $(OBJROOT)/$(NAME)
	gnutar xozf $(TARBALL) -C $(OBJROOT)/$(NAME)

install: realinstall postinstall

realinstall: $(DIR) custompatching
	cd $(DIR) && \
	ARCHFLAGS="$(RC_CFLAGS)" \
	python setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)" $(SETUPARGS)

# allow custom patching before running setup.py
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
