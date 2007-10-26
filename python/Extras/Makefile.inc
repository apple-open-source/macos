NAMEVERSION = $(NAME)-$(VERSION)
TARBALL = $(NAMEVERSION).tar.gz
DIR = $(OBJROOT)/$(NAME)/$(NAMEVERSION)

no_target: $(DIR)/.mark

$(DIR)/.mark: $(DIR) custompatching
	@set -x && \
	cd $(DIR) && \
	$(PYTHONENV) $(ENV) $(PYTHON) setup.py build
	touch $@

$(DIR):
	mkdir -p $(OBJROOT)/$(NAME)
	gnutar xzf $(TARBALL) -C $(OBJROOT)/$(NAME)

install: realinstall postinstall

realinstall: $(DIR)/.mark
	@set -x && \
	cd $(DIR) && \
	$(PYTHONENV) $(ENV) $(PYTHON) setup.py install --home=$(EXTRAS)

# allow custom patching before running Makefile.PL
custompatching:

# allow post install customization
postinstall:
