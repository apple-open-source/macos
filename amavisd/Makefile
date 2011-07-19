#
# xbs-compatible wrapper Makefile for SpamAssassin
#

PROJECT=amavisd

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=
CFLAGS=-Os $(RC_CFLAGS)

# Configuration values we customize
#

PROJECT_NAME=amavisd
PROJECT_VERS=2.6.6
PROJECT_PATH=amavisd-new-

ETC_DIR=/private/etc
LANGUAGES_DIR=$(ETC_DIR)/mail/amavisd/languages/en.lproj
AMAVIS_DIR=/private/var/amavis
AMAVIS_TMP_DIR=/private/var/amavis/tmp
AMAVIS_DB_DIR=/private/var/amavis/db
SETUP_EXTRAS_SRC_DIR=amavisd.SetupExtras
COMMON_EXTRAS_DST_DIR=/System/Library/ServerSetup/CommonExtras
VIRUS_MAILS_DIR=/private/var/virusmails
USR_BIN=/usr/bin
BIN_DIR=/amavisd.Bin
LAUNCHD_SRC_DIR=/amavisd.LaunchDaemons
LAUNCHD_DIR=/System/Library/LaunchDaemons
OS_SRC_DIR=amavisd.OpenSourceInfo
AMAVIS_CONF_DIR=/amavisd.Conf
INSTALL_EXTRAS_DIR=/amavisd.InstallExtras
USR_LOCAL=/usr/local
USR_OS_VERSION=$(USR_LOCAL)/OpenSourceVersions
USR_OS_LICENSE=$(USR_LOCAL)/OpenSourceLicenses
MAN8_SRC_DIR=/amavisd.Man
MAN8_DST_DIR=/usr/share/man/man8

STRIP=/usr/bin/strip
GNUTAR=/usr/bin/gnutar
CHOWN=/usr/sbin/chown

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: make_amavisd

install :: make_amavisd_install

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

make_amavisd_install : $(DSTROOT)$(ETC_DIR) $(DSTROOT)$(USR_BIN)
	$(SILENT) $(ECHO) "-------------- Amavisd-new --------------"

	# install launchd plist
	install -d -m 0755 "$(DSTROOT)$(LAUNCHD_DIR)"
	install -m 0644 "$(SRCROOT)$(LAUNCHD_SRC_DIR)/org.amavis.amavisd.plist" "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd.plist"
	install -m 0644 "$(SRCROOT)$(LAUNCHD_SRC_DIR)/org.amavis.amavisd_cleanup.plist" "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd_cleanup.plist"

	# install amavis config and scripts
	install -m 0644 "$(SRCROOT)/$(AMAVIS_CONF_DIR)/amavisd.conf" "$(DSTROOT)/$(ETC_DIR)/amavisd.conf.default"
	install -m 0755 "$(SRCROOT)/$(PROJECT_NAME)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd" "$(DSTROOT)/$(USR_BIN)/amavisd"
	install -m 0755 "$(SRCROOT)/$(PROJECT_NAME)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-agent" "$(DSTROOT)/$(USR_BIN)/amavisd-agent"
	install -m 0755 "$(SRCROOT)/$(PROJECT_NAME)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-nanny" "$(DSTROOT)/$(USR_BIN)/amavisd-nanny"
	install -m 0755 "$(SRCROOT)/$(PROJECT_NAME)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-release" "$(DSTROOT)/$(USR_BIN)/amavisd-release"

	# install amavis  directories
	install -d -m 0750 "$(DSTROOT)$(AMAVIS_DIR)"
	install -d -m 0750 "$(DSTROOT)$(AMAVIS_DB_DIR)"
	install -d -m 0750 "$(DSTROOT)$(AMAVIS_TMP_DIR)"
	install -d -m 0755 "$(DSTROOT)$(LANGUAGES_DIR)"
	$(SILENT) ($(CHOWN) -R _amavisd:_amavisd "$(DSTROOT)$(AMAVIS_DIR)")

	install -d -m 0750 "$(DSTROOT)$(VIRUS_MAILS_DIR)"
	$(SILENT) ($(CHOWN) -R _amavisd:_amavisd "$(DSTROOT)$(VIRUS_MAILS_DIR)")

	$(SILENT) (/bin/echo "\n" > "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")
	$(SILENT) ($(CHOWN) -R _amavisd:_amavisd "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")

	# install default language files
	install -m 0644 "$(SRCROOT)/$(INSTALL_EXTRAS_DIR)/languages/en.lproj/"* "$(DSTROOT)/$(LANGUAGES_DIR)/"

	# Setup & migration extras
	install -d -m 0755 "$(DSTROOT)$(COMMON_EXTRAS_DST_DIR)"
	install -m 0755 "$(SRCROOT)$)/$(SETUP_EXTRAS_SRC_DIR)/amavisd_common" "$(DSTROOT)/$(COMMON_EXTRAS_DST_DIR)/SetupAmavisd.sh"
	install -o _amavisd -m 0755 "$(SRCROOT)$)/$(SETUP_EXTRAS_SRC_DIR)/amavisd_cleanup" "$(DSTROOT)/$(AMAVIS_DIR)/amavisd_cleanup"

	install -d -m 0755 "$(DSTROOT)$(USR_OS_VERSION)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_LICENSE)"
	install -m 0755 "$(SRCROOT)/$(OS_SRC_DIR)/amavisd.plist" "$(DSTROOT)/$(USR_OS_VERSION)/amavisd.plist"
	install -m 0755 "$(SRCROOT)/$(OS_SRC_DIR)/amavisd.txt" "$(DSTROOT)/$(USR_OS_LICENSE)/amavisd.txt"

	install -d -m 0755 "$(DSTROOT)$(MAN8_DST_DIR)"
	install -m 0444 "$(SRCROOT)/$(MAN8_SRC_DIR)/amavisd.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd.8"
	install -m 0444 "$(SRCROOT)/$(MAN8_SRC_DIR)/amavisd-agent.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-agent.8"
	install -m 0444 "$(SRCROOT)/$(MAN8_SRC_DIR)/amavisd-nanny.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-nanny.8"
	install -m 0444 "$(SRCROOT)/$(MAN8_SRC_DIR)/amavisd-release.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-release.8"

	$(SILENT) $(ECHO) "---- Building Amavisd-new complete."

.PHONY: clean installhdrs installsrc build install 


$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETC_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(USR_BIN) :
	$(SILENT) $(MKDIRS) $@
