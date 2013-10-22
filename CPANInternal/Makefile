export SHELL := /bin/sh

##############################################################################
# Global Constants
##############################################################################

export OS_VERSION              := $(shell /usr/bin/sw_vers -buildVersion | sed -E 's/[A-Z][0-9]+[A-Za-z]*$$//g')
export OS_VERSIONS_SUPPORTED   := 11 12 13
export PERL_VERSIONS_FILE_PATH := /usr/local/versioner/perl/versions
export PERL_VERSIONS_AVAILABLE := $(sort $(shell grep -v '^DEFAULT = ' $(PERL_VERSIONS_FILE_PATH)))

ifeq ($(OS_VERSION), 13)
    # 5.16 is manually added to ensure it is included in the list of supported
    # Perl versions
    PERL_VERSIONS_AVAILABLE := $(sort 5.16 $(PERL_VERSIONS_AVAILABLE))
endif

##############################################################################
# XBS Targets
##############################################################################

.PHONY: clean
clean:

.PHONY: install_config
install_config::
	@for perl_version in $(PERL_VERSIONS_AVAILABLE); do \
		export VERSIONER_PERL_VERSION=$$perl_version; \
		objRootPerl=$(OBJROOT)/$$perl_version; \
		echo "OBJROOT_PERL: $$objRootPerl"; \
		$(MAKE) -C Makefiles prepend-to-path OBJROOT_PERL=$$objRootPerl || exit 1; \
	done;

.PHONY: installsrc
installsrc::
	ditto . $(SRCROOT)
	for name in .DS_Store .git .gitignore .svn CVS; do \
		find $(SRCROOT) -name $$name -prune -exec rm -rf {} \; ; \
	done

.PHONY: installhdrs
installhdrs::

.PHONY: install
install::
	@for perl_version in $(PERL_VERSIONS_AVAILABLE); do \
		export VERSIONER_PERL_VERSION=$$perl_version; \
		objRootPerl=$(OBJROOT)/$$perl_version; \
		mkdir -p $$objRootPerl; \
		if [ "$(SRCROOT)" != "$$objRootPerl" ]; then \
			ditto $(SRCROOT) $$objRootPerl; \
		fi; \
		cd $$objRootPerl && ./applyPatches; \
		$(MAKE) -C Makefiles OBJROOT_PERL=$$objRootPerl || exit 1; \
	done;
