# This file is include in the sub-projects's Makefiles to inherit standard
# build behavior.  The sub-project's Makefiles need to define NAME and
# VERSION.  The custompatching target may by redefined to do any patching
# just after files are unpacked from the tarball.  The postinstall target
# may be redefined to do any post install processing.

NAMEVERSION = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERSION).tar.gz
DIR = $(OBJROOT)/$(NAME)/$(NAMEVERSION)
export PATH := $(DSTROOT)/usr/bin:$(PATH)

no_target: $(DIR)/.mark

$(DIR)/.mark: $(DIR) custompatching
	@echo cd $(DIR) && \
	cd $(DIR) && \
	echo $(ENV) perl Makefile.PL $(PLARGS) && \
	$(ENV) perl Makefile.PL $(PLARGS) && \
	echo $(ENV) make $(MAKEARGS) && \
	$(ENV) make $(MAKEARGS)
	touch $@

$(DIR):
	mkdir -p $(OBJROOT)/$(NAME)
	gnutar xzf $(TARBALL) -C $(OBJROOT)/$(NAME)

install: realinstall postinstall

realinstall: $(DIR)/.mark
	@echo cd $(DIR) && \
	cd $(DIR) && \
	echo $(ENV) make install $(MAKEARGS) && \
	$(ENV) make install $(MAKEARGS)

# allow custom patching before running Makefile.PL
custompatching:

# allow post install customization
postinstall:

plist:
	@echo $(NAME)
	@echo $(VERSION)
	@echo $(TARBALL)
