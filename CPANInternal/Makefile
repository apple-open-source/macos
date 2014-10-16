export SHELL := /bin/sh

##############################################################################
# Global Constants
##############################################################################

export OS_VERSION              := $(shell /usr/bin/sw_vers -buildVersion | sed -E 's/[A-Z][0-9]+[A-Za-z]*$$//g')
export PERL_VERSIONS_FILE_PATH := /usr/local/versioner/perl/versions
PERL_VERSIONS_FILE_PATH := $(or $(join $(SDKROOT),$(PERL_VERSIONS_FILE_PATH)),$(PERL_VERSIONS_FILE_PATH))

export PERL_VERSIONS_AVAILABLE := $(sort $(shell grep -v '^DEFAULT = ' $(PERL_VERSIONS_FILE_PATH)))

# Allow CPANInternal to build on Mac OS X versions greater than 13.
#ifeq ($(shell /bin/test $(OS_VERSION) -gt 13; echo $$?), 0)
#    OS_VERSION := 13
#endif

ifeq ($(OS_VERSION), 14)
    # 5.18 is manually added to ensure it is included in the list of supported
    # Perl versions
    PERL_VERSIONS_AVAILABLE := $(sort 5.18 $(PERL_VERSIONS_AVAILABLE))
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
