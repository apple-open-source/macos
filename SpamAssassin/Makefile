#
# xbs-compatible wrapper Makefile for SpamAssassin
#

Project		= SpamAssassin
OPEN_SOURCE_VER	= 3.3.2
PROJECT_VERSION	= $(Project)-$(OPEN_SOURCE_VER)

# Configuration values we customize
#
OPEN_SOURCE_DIR	= Mail-$(PROJECT_VERSION)
SPAM_TAR_GZ		= $(OPEN_SOURCE_DIR).tar.gz

PROJECT_BIN_DIR	 = $(Project).Bin
PROJECT_CONF_DIR = $(Project).Config
PROJECT_LD_DIR	 = $(Project).LaunchDaemons
PROJECT_OS_DIR	 = $(Project).OpenSourceInfo
PROJECT_PATCH_DIR	= $(Project).Patch
PROJECT_SETUP_DIR	= $(Project).SetupExtras

CONFIG_DIR	 = $(ETCDIR)/mail/spamassassin
V310_PRE	 = $(DSTROOT)$(CONFIG_DIR)/v310.pre
V310_PRE_TMP = $(DSTROOT)$(CONFIG_DIR)/v310.pre.tmp

CONFIG_ENV	= MAKEOBJDIR="$(BuildDirectory)" \
            INSTALL_ROOT="$(DSTROOT)" \
            TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"

CFLAGS		= -g -Os $(RC_CFLAGS)
LDFLAGS		= $(CFLAGS)

DSYMUTIL=/usr/bin/dsymutil

# multi-version support
VERSIONER_DIR := /usr/local/versioner

# Perl multi-version support
PERL_VERSIONS := $(VERSIONER_DIR)/perl/versions
PERL_SUB_DEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERL_VERSIONS))
PERL_DEFAULT := $(shell grep '^$(PERL_SUB_DEFAULT)' $(PERL_VERSIONS))
PERL_UNORDERED_VERS := $(shell grep -v '^DEFAULT' $(PERL_VERSIONS))

# do default version last
PERL_BUILD_VERS := $(filter-out $(PERL_DEFAULT),$(PERL_UNORDERED_VERS)) $(PERL_DEFAULT)

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= MAKEOBJDIR="$(BuildDirectory)" \
            INSTALL_ROOT="$(DSTROOT)" \
            TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"

# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment =

Make_Flags	=

ProjectConfig		= $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/$(Project)-config

Common_Configure_Flags	= Makefile.PL PREFIX=/ \
							INSTALLSITEDATA=$(VARDIR)/spamassassin/3.003002 \
							INSTALLSITECONF=$(ETCDIR)/mail/spamassassin \
							ENABLE_SSL=yes

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

XCODEBUILD		= $(USRBINDIR)/xcodebuild

# Override settings from above includes
BUILD_DIR		= $(OBJROOT)/Build
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
TMPDIR			= $(OBJROOT)/Build/tmp

# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"

# Typically defined in GNUSource.make; duplicated here to effect similar functionality.
Sources				= $(SRCROOT)
Configure			= perl
ConfigureProject	= $(Configure)
ProjectConfigStamp	= $(BuildDirectory)/$(Project)/configure-stamp

LIB_TOOL			= $(BuildDirectory)/$(Project)/libtool

.PHONY: build-sa
.PHONY: archive-strip-binaries install-extras install-man install-startup-files
.PHONY: install-open-source-files

default : clean build-sa

install :: build-sa install-extras install-startup-files \
			do-cleanup normalize-directories archive-strip-binaries

install-no-clean :: build-sa

###################

build-sa : $(BUILD_DIR) $(TMPDIR)
	@echo "***** Building $(Project)"
	$(_v) for vers in $(PERL_BUILD_VERS); do \
		export VERSIONER_PERL_VERSION=$${vers}; \
		cd $(BuildDirectory) && gnutar -xzpf $(Sources)/$(PROJECT_BIN_DIR)/$(SPAM_TAR_GZ); \
		$(MV) $(BuildDirectory)/$(OPEN_SOURCE_DIR) $(BuildDirectory)/$(Project)-$${vers}; \
		cd "$(BuildDirectory)/$(Project)-$${vers}" && \
				$(PATCH) -p1 < "$(SRCROOT)/$(PROJECT_PATCH_DIR)/patch-1.diff"; \
		cd $(BuildDirectory)/$(Project)-$${vers} && \
				$(Environment) $(ConfigureProject) $(Common_Configure_Flags) \
					LIB=/System/Library/Perl/Extras/$${vers} PERL_VERSION=$${vers}; \
		$(_v) $(MAKE) -C $(BuildDirectory)/$(Project)-$${vers} CFLAGS="$(CFLAGS)" $(Make_Flags) $(Install_Flags) $(Install_Target); \
	done
	@echo "***** Building $(Project) complete."

$(ProjectConfig): $(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config
	$(_v) $(CP) "$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config" $@
$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config:
	$(_v) $(MAKE) build-sa

# Custom configuration

do-cleanup :
	@echo "***** Cleaning up files not intended for installation"
	# remove installed local.cf file
	$(_v) $(RM) $(DSTROOT)$(CONFIG_DIR)/local.cf

	# cleanup perl directories
	$(_v) find $(DSTROOT) -name \*.bs -delete
	$(_v) find $(DSTROOT) -name perllocal.pod -delete
	$(_v) find $(DSTROOT) -type d -empty -delete
	$(_v) find $(DSTROOT) -name darwin-thread-multi-2level -delete
	$(_v) $(RMDIR) $(SYMROOT)$(ETCDIR)
	$(_v) $(RMDIR) $(SYMROOT)$(SHAREDIR)
	@echo "***** Cleaning up complete."

archive-strip-binaries: $(SYMROOT)
	@echo "***** Archiving, dSYMing and stripping binaries..."
	$(_v) for file in $(DSTROOT)$(USRBINDIR)/spamc;\
	do \
		echo "Processing $${file##*/} (from $${file})";	\
		$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		$(STRIP) -S $${file};	\
	done
	@echo "***** Archiving, dSYMing and stripping binaries complete."

normalize-directories :
	@echo "***** Making standard directory paths..."
	# Create & merge into /private/etc
	$(_v) if [ ! -d "$(DSTROOT)$(ETCDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(ETCDIR)";		\
		$(MKDIR) "$(DSTROOT)$(ETCDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)/etc" -a "$(ETCDIR)" != "/etc" ]; then	\
		echo "$(MV) $(DSTROOT)/etc/* $(DSTROOT)$(ETCDIR)";	\
		$(MV) "$(DSTROOT)/etc/"* "$(DSTROOT)$(ETCDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)/etc";	\
		$(RMDIR) "$(DSTROOT)/etc";	\
	fi

	# Create & merge into /usr/bin
	$(_v) if [ ! -d "$(DSTROOT)$(USRBINDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(USRBINDIR)";		\
		$(MKDIR) "$(DSTROOT)$(USRBINDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)/bin" -a "$(USRBINDIR)" != "/bin" ]; then	\
		echo "$(MV) $(DSTROOT)/bin/* $(DSTROOT)$(USRBINDIR)";	\
		$(MV) "$(DSTROOT)/bin/"* "$(DSTROOT)$(USRBINDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)/bin";	\
		$(RMDIR) "$(DSTROOT)/bin";	\
	fi

	# Create & merge into /usr/share
	$(_v) if [ ! -d "$(DSTROOT)$(SHAREDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(SHAREDIR)";		\
		$(MKDIR) "$(DSTROOT)$(SHAREDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)/share" -a "$(SHAREDIR)" != "/share" ]; then	\
		echo "$(MV) $(DSTROOT)/share/* $(DSTROOT)$(SHAREDIR)";	\
		$(MV) "$(DSTROOT)/share/"* "$(DSTROOT)$(SHAREDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)/share";	\
		$(RMDIR) "$(DSTROOT)/share";	\
	fi
	@echo "***** Making standard directory paths complete."

install-open-source-files :
	@echo "***** Installing open source configuration files..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceVersions
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceLicenses
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).plist" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceVersions"
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).txt" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceLicenses"
	@echo "***** Installing open source configuration files complete."

install-extras : install-open-source-files
	@echo "***** Installing extras..."
	# install directories
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CommonExtras

	# Service configuration file
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_CONF_DIR)/local.cf.default \
			$(DSTROOT)$(CONFIG_DIR)/local.cf.default

	# Service setup script
	$(_v) $(INSTALL_SCRIPT) $(SRCROOT)/$(PROJECT_SETUP_DIR)/SetupSpamAssassin.sh \
			$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CommonExtras/SetupSpamAssassin.sh

	# Service runtime scripts
	$(_v) $(INSTALL_SCRIPT) $(SRCROOT)/$(PROJECT_SETUP_DIR)/learn_junk_mail \
			$(DSTROOT)$(CONFIG_DIR)/learn_junk_mail.sh
	$(_v) $(INSTALL_SCRIPT) $(SRCROOT)/$(PROJECT_SETUP_DIR)/sa_update \
			$(DSTROOT)$(CONFIG_DIR)/sa_update.sh
	$(_v) $(SED) -e 's/#loadplugin Mail::SpamAssassin::Plugin::TextCat/loadplugin Mail::SpamAssassin::Plugin::TextCat/' \
			"$(V310_PRE)" > "$(V310_PRE_TMP)"
	$(_v) $(RM) "$(V310_PRE)"
	$(_v) $(MV) "$(V310_PRE_TMP)" "$(V310_PRE)"
	@echo "***** Installing extras complete."

install-startup-files :
	@echo "***** Installing Startup Item..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_LD_DIR)/com.apple.salearn.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.salearn.plist
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_LD_DIR)/com.apple.updatesa.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.updatesa.plist
	@echo "***** Installing Startup Item complete."

$(DSTROOT) $(TMPDIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi

$(OBJROOT) $(BUILD_DIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi
