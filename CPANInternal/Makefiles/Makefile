export SHELL := /bin/sh

##############################################################################
# Global Constants
##############################################################################

export OS_VERSION              := $(shell /usr/bin/sw_vers -buildVersion | sed -E 's/[A-Z][0-9]+[A-Za-z]*$$//g')
export OS_VERSIONS_SUPPORTED   := 11 12 13
export PERL                    := /usr/bin/perl
export PERL_VERSIONS_FILE_PATH := /usr/local/versioner/perl/versions
export PERL_VERSIONS_AVAILABLE := $(sort $(shell grep -v '^DEFAULT = ' $(PERL_VERSIONS_FILE_PATH)))

# Complete list of projects
PROJECTS = $(MAKEMAKER_PROJECTS) $(MODULE_BUILD_PROJECTS)

##############################################################################
# Operating System Environment
##############################################################################

# Verify the Makefile is being executed on a supported version of Mac OS X.
ifeq ($(findstring $(OS_VERSION), $(OS_VERSIONS_SUPPORTED)),)
    $(error Makefile is being executed on an unsupported version of Mac OS X. Supported versions are $(OS_VERSIONS_SUPPORTED))
endif

# Load Makefile containing definitions specific to the host OS
include $(OS_VERSION)/Makefile

# Assign ARCHFLAGS the value of DEFAULT_ARCHFLAGS defined in $(OS_VERSION)/Makefile
ARCHFLAGS = $(DEFAULT_ARCHFLAGS)

# If set RC_CFLAGS should override DEFAULT_ARCHFLAGS
ifneq ($(RC_CFLAGS),)
    $(info RC_FLAGS is set, overriding DEFAULT_ARCHFLAGS)
    ARCHFLAGS = $(RC_CFLAGS)
endif

# Load Makefile containing definitions specific for the current version of Perl
include_file_paths := $(OS_VERSION)/Perl/$(VERSIONER_PERL_VERSION)/Makefile
include $(include_file_paths)

##############################################################################
# Perl Environment
##############################################################################

# Tests that require interaction with a user typically respect the variable 
# AUTOMATED_TESTING. When AUTOMATED_TESTING is set, interactive tests are skipped. This
# helps with things like Term::ReadLine waiting for someone to type something in, when it
# will never happen on a build machine.
export AUTOMATED_TESTING := 1
export PERL_AUTOINSTALL  := --skipdeps

# ExtUtils::MakeMaker
# The install target copies files found below each of the INST_* directories to their 
# INSTALL* counterparts. Setting INSTALLDIRS to "site" to chooses the INSTALLSITE*
# counterparts.
#
# http://perldoc.perl.org/ExtUtils/MakeMaker.html#make-install
PERL_INSTALLDIRS         := site

# Retrieve the SITEPREFIX INSTALLSITE* counterparts
PERL_INSTALLSITEARCH     := $(shell $(PERL) -MConfig -e 'print($$Config{installsitearch});')
PERL_INSTALLSITEBIN      := $(shell $(PERL) -MConfig -e 'print($$Config{installsitebin});')
PERL_INSTALLSITELIB      := $(shell $(PERL) -MConfig -e 'print($$Config{installsitelib});')
PERL_INSTALLSITEMAN1DIR  := $(shell $(PERL) -MConfig -e 'print($$Config{installsiteman1dir});')
PERL_INSTALLSITEMAN3DIR  := $(shell $(PERL) -MConfig -e 'print($$Config{installsiteman3dir});')
PERL_INSTALLSITESCRIPT   := $(shell $(PERL) -MConfig -e 'print($$Config{installsitescript});')
PERL_PREPENDTOPATH       := $(PERL_INSTALLSITELIB)PrependToPath

INSTALLPATH_PREPENDTOPATH := $(DSTROOT)$(PERL_INSTALLSITELIB)

# Modify PERL_INSTALLSITEARCH and PERL_INSTALLSITELIB so files are installed into 
# /AppleInternal/Library/Perl
PERL_INSTALLSITEARCH     := /AppleInternal$(PERL_INSTALLSITEARCH)
PERL_INSTALLSITELIB      := /AppleInternal$(PERL_INSTALLSITELIB)

# Append install paths
PERL += -I$(DSTROOT)$(PERL_INSTALLSITEARCH)
PERL += -I$(DSTROOT)$(PERL_INSTALLSITELIB)

##############################################################################
# Module::Build Environment Variables
#
# http://perldoc.perl.org/Module/Build.html#Environment-variables
##############################################################################

# Command line options that are applied to Build.PL or any Build action.
PERL_MB_OPT := \
--destdir=$(DSTROOT) \
--installdirs $(PERL_INSTALLDIRS) \
--install_path arch=$(PERL_INSTALLSITEARCH) \
--install_path bin=$(PERL_INSTALLSITEBIN) \
--install_path lib=$(PERL_INSTALLSITELIB) \
--install_path bindoc=$(PERL_INSTALLSITEMAN1DIR) \
--install_path libdoc=$(PERL_INSTALLSITEMAN3DIR) \
--install_path script=$(PERL_INSTALLSITESCRIPT)

##############################################################################
# ExtUtils::MakeMaker Environment Variables
#
# http://perldoc.perl.org/ExtUtils/MakeMaker.html#ENVIRONMENT
##############################################################################

PERL_MM_OPT := \
DESTDIR=$(DSTROOT) \
INSTALLDIRS=$(PERL_INSTALLDIRS) \
INSTALLSITEARCH=$(PERL_INSTALLSITEARCH) \
INSTALLSITEBIN=$(PERL_INSTALLSITEBIN) \
INSTALLSITELIB=$(PERL_INSTALLSITELIB) \
INSTALLSITEMAN1DIR=$(PERL_INSTALLSITEMAN1DIR) \
INSTALLSITEMAN3DIR=$(PERL_INSTALLSITEMAN3DIR) \
INSTALLSITESCRIPT=$(PERL_INSTALLSITESCRIPT)

# Instruct the ExtUtils::MakeMaker prompt function to always return the default without 
# waiting for user input.
export PERL_MM_USE_DEFAULT = 1

##############################################################################
# General Targets
##############################################################################

.PHONY: all
all: display-info $(MODULES) ConfigurationFiles remove-symbols
	@echo "";

.PHONY: clean
clean:

.PHONY: display-info
display-info:
	@echo ""; \
	echo "##############################################################################"; \
	echo "# Variables"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "ARCHFLAGS:               $(ARCHFLAGS)"; \
	echo "OS_VERSION:              $(OS_VERSION)"; \
	echo "OS_VERSIONS_SUPPORTED:   $(OS_VERSIONS_SUPPORTED)"; \
	echo "PERL_VERSIONS_AVAILABLE: $(PERL_VERSIONS_AVAILABLE)"; \
	echo "PERL_INSTALLSITEARCH:    $(PERL_INSTALLSITEARCH)"; \
	echo "PERL_INSTALLSITEBIN:     $(PERL_INSTALLSITEBIN)"; \
	echo "PERL_INSTALLSITELIB:     $(PERL_INSTALLSITELIB)"; \
	echo "PERL_INSTALLSITEMAN1DIR: $(PERL_INSTALLSITEMAN1DIR)"; \
	echo "PERL_INSTALLSITEMAN3DIR: $(PERL_INSTALLSITEMAN3DIR)"; \
	echo "PERL_INSTALLSITESCRIPT:  $(PERL_INSTALLSITESCRIPT)"; \
	echo ""; \
	echo "Makefiles:               $(include_file_paths)"; \
	echo ""; \
	echo "##############################################################################"; \
	echo "# Module::Build Variables"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "PERL_MB_OPT:             $(PERL_MB_OPT)"; \
	echo ""; \
	echo "##############################################################################"; \
	echo "# ExtUtils::MakeMaker Variables"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "PERL_MM_OPT:             $(PERL_MM_OPT)"; \
	echo "PERL_MM_USE_DEFAULT:     $$PERL_MM_USE_DEFAULT"; \
	echo ""; \
	echo "##############################################################################"; \
	echo "# Shell Environment Variables"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "PERL5LIB:                $$PERL5LIB"; \
	echo "VERSIONER_PERL_VERSION:  $$VERSIONER_PERL_VERSION"; \
	echo ""; \
	echo "##############################################################################"; \
	echo "# XBS Variables"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "DSTROOT:                 $(DSTROOT)"; \
	echo "OBJROOT:                 $(OBJROOT)"; \
	echo "OBJROOT_PERL:            $(OBJROOT_PERL)"; \
	echo "SRCROOT:                 $(SRCROOT)"; \
	echo "SYMROOT:                 $(SYMROOT)"; \
	echo "TMPDIR:                  $(TMPDIR)"; \
	echo ""; \
	echo "##############################################################################"; \
	echo "# Projects"; \
	echo "##############################################################################"; \
	echo ""; \
	echo "$(PROJECTS)"; \
	echo ""

.PHONY: ConfigurationFiles
ConfigurationFiles:
	@echo "=============== Making $@ ==================";
	@echo "";
	$(MAKE) -C $(OBJROOT_PERL)/$@ install;

.PHONY: prepend-to-path
prepend-to-path:
	@mkdir -p $(INSTALLPATH_PREPENDTOPATH); \
	echo $(PERL_INSTALLSITEARCH) > $(INSTALLPATH_PREPENDTOPATH)/PrependToPath; \
	echo $(PERL_INSTALLSITELIB) >> $(INSTALLPATH_PREPENDTOPATH)/PrependToPath;

.PHONY: remove-symbols
remove-symbols:
	@if [ -e $(DSTROOT) ]; then \
		echo "Removing debugging symbol table entries from bundles ... "; \
		find $(DSTROOT) -xdev -name '*.bundle' -print -exec strip -S {} \; ; \
		echo ""; \
		echo "Removing packlists ..."; \
		find $(DSTROOT) -xdev -name '.packlist' -print -exec rm -f {} \; ; \
		echo ""; \
	fi

##############################################################################
# ExtUtils::MakeMaker Project Targets
##############################################################################

.PHONY: $(MAKEMAKER_PROJECTS)
$(MAKEMAKER_PROJECTS) :
	@echo "=============== Making $@ =================="; \
	echo ""; \
	cd $(OBJROOT_PERL)/$@ && \
	mv Makefile.PL Makefile.PL.orig && \
	cat Makefile.PL.orig ../add_rc_constants.pl > Makefile.PL && \
	$(PERL) Makefile.PL PERL="$(PERL)" $(PERL_MM_OPT) && \
	$(MAKE) all test pure_install || exit 1; \
	echo "";

##############################################################################
# Module::Build Project Targets
##############################################################################

.PHONY: $(MODULE_BUILD_PROJECTS)
$(MODULE_BUILD_PROJECTS) :
	@echo "=============== Making $@ =================="; \
	echo ""; \
	cd $(OBJROOT_PERL)/$@ && \
	$(PERL) Build.PL $(PERL_MB_OPT) && \
	$(PERL) Build build $(PERL_MB_OPT) && \
	$(PERL) Build test $(PERL_MB_OPT) && \
	$(PERL) Build install $(PERL_MB_OPT) || exit 1; \
	echo "";
