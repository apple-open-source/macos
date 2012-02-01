##
# Apple wrapper Makefile for Roundcube Webmail
# B&I project name: WebmailServer
# Radar component: Webmail / X Server
# Copyright (c) 2010-2011 Apple Inc. All rights reserved.
##
# To do
# - Allow connection to other mail servers

# General project info for use with RC/GNUsource.make makefile
Project         = roundcubemail
ProjectName     = WebmailServer
UserType        = Administrator
ToolType        = Commands
Submission      = 11

# Project installation tree
PROJECT_DIR=$(SHAREDIR)/webmail
CONFIG_DIR=$(PROJECT_DIR)/config
IMAGES_DIR=$(PROJECT_DIR)/skins/default/images
LOG_DIR=$(LOGDIR)/webmail
TEMP_DIR=$(VARDIR)/webmail

# Apple-specific versions of project files
LOGO=web_mail_login.png
WATERMARK=watermark.gif
CONFIG_MAIN=main.inc.php
CONFIG_DB=db.inc.php
CONFIG_APPLE_OVERRIDES=appleoverrides.inc.php

# Support files
HTTPD_INCLUDE_FILE=httpd_webmailserver.conf
HTTPD_CONF_DIR=$(ETCDIR)/apache2
WEBAPP_PLIST=com.apple.webapp.webmailserver.plist
WEBAPP_DIR=$(HTTPD_CONF_DIR)/webapps

SETUP_FILE=WebmailServerSetup.sh
SETUP_DIR=$(NSLIBRARYDIR)/ServerSetup/CommonExtras/PostgreSQLExtras


# Additional project info used with AEP
AEP		= YES
AEP_Version	= 0.6
AEP_Patches	= editor.patch key.patch locstrings.patch \
			identity.patch anti_XSS.patch IPv6.patch
#AEP_ConfigDir	= $(PROJECT_DIR)/config
# Disable all the GNU options because this project doesn't use any configure or a Makefile
GnuNoConfigure = YES
GnuNoBuild = YES
GnuNoInstall = YES
GnuAfterInstall	= configure-project install-project install-loc install-macosx

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

# Build rules
configure-project:
	@echo "Customizing configuration of $(ProjectName)..."
	$(CP) $(SRCROOT)/plugins/* $(Sources)/plugins
	$(CP) $(Sources)/config/$(CONFIG_MAIN).dist $(Sources)/config/$(CONFIG_MAIN)
	$(CP) $(Sources)/config/$(CONFIG_DB).dist $(Sources)/config/$(CONFIG_DB)
	echo "// Add Apple-specific require directive" >> $(Sources)/config/$(CONFIG_DB)
	echo "require_once(RCMAIL_CONFIG_DIR . '/$(CONFIG_APPLE_OVERRIDES)');" >> $(Sources)/config/$(CONFIG_DB)
	$(CP) $(SRCROOT)/$(CONFIG_APPLE_OVERRIDES) $(Sources)/config
	@echo "Clearing execute permission..."
	$(_v) $(FIND) $(Sources) -type f -perm +0111 '(' -name '*.php' -o -name '*.js' -o -name '*.inc' ')' -exec $(CHMOD) a-x "{}" \;
	@echo "Configuration complete."

# The FIND and CHOWN are necessary since we don't let GNUSource do it for us.
install-project: configure-project
	@echo "Installing $(ProjectName)..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(PROJECT_DIR)
	$(CP) $(Sources)/ $(DSTROOT)$(PROJECT_DIR)
	$(_v)- $(RMDIR) $(DSTROOT)$(PROJECT_DIR)/temp
	$(_v)- $(RMDIR) $(DSTROOT)$(PROJECT_DIR)/logs
	$(_v)- $(RMDIR) $(DSTROOT)$(PROJECT_DIR)/installer
	$(_v) $(FIND) $(DSTROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
	$(_v) $(FIND) $(SYMROOT) $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;
	$(_v)- $(CHOWN) -R $(Install_User):$(Install_Group) $(DSTROOT) $(SYMROOT)
	$(INSTALL) -m 0750 -o _www -g $(Install_Directory_Group) -d $(DSTROOT)$(LOG_DIR)
	$(INSTALL) -m 0750 -o _www -g $(Install_Directory_Group) -d $(DSTROOT)$(TEMP_DIR)
	@echo "Project content installed."

install-loc: install-project
	@echo "Installing Apple-generated localiation files..."
	$(_v) $(CP) $(SRCROOT)/plugins/managesieve $(DSTROOT)$(PROJECT_DIR)/plugins
	$(_v) $(CHOWN) -R root:wheel $(DSTROOT)$(PROJECT_DIR)/plugins/managesieve
	@echo "Localizations installed."

install-macosx: install-project
	@echo "Installing Apple-specific versions of project files..."
	$(INSTALL_FILE) $(LOGO) $(DSTROOT)$(IMAGES_DIR)/roundcube_logo.png
	$(INSTALL_FILE) $(WATERMARK) $(DSTROOT)$(IMAGES_DIR)/watermark.gif
	@echo "Installing support files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(HTTPD_CONF_DIR)
	$(INSTALL_FILE) $(HTTPD_INCLUDE_FILE) $(DSTROOT)$(HTTPD_CONF_DIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(WEBAPP_DIR)
	$(INSTALL_FILE) $(WEBAPP_PLIST) $(DSTROOT)$(WEBAPP_DIR)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SETUP_DIR)
	$(INSTALL_PROGRAM) $(SETUP_FILE) $(DSTROOT)$(SETUP_DIR)
	$(_v) /usr/bin/find "$(DSTROOT)$(PROJECT_DIR)/plugins/jqueryui" -name "*.png" | xargs chmod 644
	@echo "Install complete."
