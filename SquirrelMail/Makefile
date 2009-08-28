##
# Apple wrapper Makefile for SquirrelMail
# Copyright (c) 2002-2005 by Apple Computer, Inc.
##
# Although it is a GNU-like project, it does not come with a Makefile,
# and the configure script requires user interaction. This Makefile just provides
# the targets required by Apple's build system, and creates a config file
# config.php by modifying the default config file config_default.php
#
# This Makefile tries to conform to hier(7) by moving config, data, attachments
# to appropriate places in the file system, and makes symlinks where necessary.

PROJECT_NAME=squirrelmail
PROJECT_VERSION=1.4.17ja
PROJECT_DIR=$(PROJECT_NAME)-$(PROJECT_VERSION)
PROJECT_ARCHIVE=$(PROJECT_DIR).tar.gz
PROJECT_LOCALE_ARCHIVE=all_locales-1.4.13-20071220.tar.gz
VERSIONS_DIR=/usr/local/OpenSourceVersions
LICENSE_DIR=/usr/local/OpenSourceLicenses


# Configuration values we customize
#
IMAP_SERVER_TYPE=cyrus
IMAP_AUTH_MECH=cram-md5
DRAFT_FOLDER=Drafts
SENT_FOLDER=Sent Messages
TRASH_FOLDER=Deleted Messages
LOGO=web_mail_login.jpg
ORG_NAME=Mac OS X Server WebMail
MOTD=Mac OS X Server WebMail
CONFIG_DIR=/private/etc/$(PROJECT_NAME)
CONFIG_DEFAULT_FILE=config_default_apple.php
CONFIG_FILE=config.php

ifeq ($(MACOSX_DEPLOYMENT_TARGET), 10.6)
	HTTPD_CONF_DST=$(DSTROOT)/private/etc/apache2
else
	HTTPD_CONF_DST=$(DSTROOT)/private/etc/httpd
endif

HTTPD_DEFAULT_CONF_FILE=httpd_squirrelmail_default.conf
HTTPD_CONF_FILE=httpd_squirrelmail.conf
DATA_DIR=/private/var/db/$(PROJECT_NAME)/data/
ATTACHMENT_DIR=/private/var/db/$(PROJECT_NAME)/attachments/
SHARE_DIR=/usr/share/$(PROJECT_NAME)
PROJECT_DST=$(DSTROOT)
DATA_DIR_FULL=$(DSTROOT)/$(DATA_DIR)
ATTACHMENT_DIR_FULL=$(DSTROOT)/$(ATTACHMENT_DIR)
CONFIG_DIR_FULL=$(DSTROOT)/$(CONFIG_DIR)
SHARE_DIR_FULL=$(DSTROOT)/$(SHARE_DIR)
IMAGES_DIR_FULL=$(SHARE_DIR_FULL)/images
HELP_DIR_FULL=$(SHARE_DIR_FULL)/help
LOCALE_DIR_FULL=$(SHARE_DIR_FULL)/locale
TMP_FILE=$(OBJROOT)/tmp-file
SETUP_DIR_FULL=$(DSTROOT)/$(SYSTEM_LIBRARY_DIR)/ServerSetup/SetupExtras
SETUP_FILE=squirrelmailsetup
NEW_INDEX_FILE=index.php
JA_PATCH_FILE=1.4.17_ja_4223782_fix.patch
PROJECT_FILES=Makefile $(LOGO) $(HTTPD_CONF_FILE) $(SETUP_FILE) $(NEW_INDEX_FILE) $(PROJECT_ARCHIVE) $(PROJECT_LOCALE_ARCHIVE) SquirrelMail.plist SquirrelMail.txt $(JA_PATCH_FILE)
SRC_DIR_FULL=$(SHARE_DIR_FULL)/src
PATCH=/usr/bin/patch

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

# Set up our variables

#SILENT=@
GNUTAR=gnutar
INSTALL=/usr/bin/install
DITTO=/usr/bin/ditto

# Build rules

default:: do_untar

install:: configure do_install

clean:: do_clean

configure:: do_untar do_configure

installhdrs:: do_installhdrs

installsrc:: do_installsrc

do_untar:
	$(SILENT) $(ECHO) "Untarring $(PROJECT_NAME)..."
	$(GNUTAR) -xzf $(PROJECT_ARCHIVE)
	$(GNUTAR) -xzf $(PROJECT_LOCALE_ARCHIVE)

# Custom configuration:
#
do_configure: 
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e $(PROJECT_DIR)/config/$(CONFIG_FILE) ]; then\
		$(SED) < $(PROJECT_DIR)/config/config_default.php > $(PROJECT_DIR)/config/$(CONFIG_FILE)\
		-e 's%^\$$imap_server_type[ \t].*%$$imap_server_type = "$(IMAP_SERVER_TYPE)";%' \
		-e 's%^\$$imap_auth_mech[ \t].*%$$imap_auth_mech = "$(IMAP_AUTH_MECH)";%' \
		-e 's%^\$$org_name[ \t].*%$$org_name = "$(ORG_NAME)";%' \
		-e 's%^\$$org_logo_width[ \t].*%$$org_logo_width = 0;%' \
		-e 's%^\$$org_logo_height[ \t].*%$$org_logo_height = 0;%' \
		-e 's%^\$$org_logo[ \t].*%$$org_logo = "../images/$(LOGO)";%' \
		-e 's%^\?>%%' \
		-e 's%^\$$motd[ \t].*%$$motd = "$(MOTD)";%' \
		-e 's%^\$$trash_folder[ \t].*%$$trash_folder = "$(TRASH_FOLDER)";%' \
		-e 's%^\$$sent_folder[ \t].*%$$sent_folder = "$(SENT_FOLDER)";%' \
		-e 's%^\$$draft_folder[ \t].*%$$draft_folder = "$(DRAFT_FOLDER)";%' \
		-e 's%^\$$data_dir[ \t].*%$$data_dir = "$(DATA_DIR)";%' \
		-e 's%^\$$attachment_dir[ \t].*%$$attachment_dir = "$(ATTACHMENT_DIR)";%' \
		-e 's%^\$$domain[ \t].*%$$domain = getenv(SERVER_NAME);%' \
		-e 's%^\$$auto_expunge[ \t].*%$$auto_expunge = false;%' \
		; \
		echo '/* Whether to hide references to SquirrelMail on login and other pages */'  >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo 'global $$hide_sm_attributions;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$hide_sm_attributions = true;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '/* Additional required defaults missing from config_default.php */' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$config_use_color = 2;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$provider_uri = "http://www.squirrelmail.org/";' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$provider_name = SquirrelMail;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$prefs_user_field = user;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$prefs_key_field = prefkey;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$prefs_val_field = prefval;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$allow_charset_search = true;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$uid_support = true;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$default_use_mdn = true;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$pop_before_smtp = false;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$optional_delimiter = detect;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$default_use_priority = true;' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '$$theme_css = "";' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		echo '?>' >> $(PROJECT_DIR)/config/$(CONFIG_FILE); \
		cp $(PROJECT_DIR)/config/$(CONFIG_FILE) $(PROJECT_DIR)/config/$(CONFIG_DEFAULT_FILE); \
	fi

do_install: $(DSTROOT) $(HTTPD_CONF_DST) $(DATA_DIR_FULL) $(CONFIG_DIR_FULL) $(SHARE_DIR_FULL) $(ATTACHMENT_DIR_FULL) $(SETUP_DIR_FULL) $(DSTROOT)$(VERSIONS_DIR) $(DSTROOT)$(LICENSE_DIR)
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME)..."
	$(SILENT) $(CHMOD) -R ugo-s $(PROJECT_DIR)/*
	$(SILENT) $(CP) -r $(PROJECT_DIR)/* $(SHARE_DIR_FULL)
	$(SILENT) $(CP) index.php $(SRC_DIR_FULL)
	$(SILENT) $(MV) $(SHARE_DIR_FULL)/config $(CONFIG_DIR_FULL)
	$(SILENT) $(CD) $(CONFIG_DIR_FULL); ln -s $(SHARE_DIR)/plugins .
	$(SILENT) $(MV) $(SHARE_DIR_FULL)/data/default_pref $(DATA_DIR_FULL)
	$(SILENT) $(CD) $(SHARE_DIR_FULL); ln -s $(CONFIG_DIR)/config .
	$(SILENT) $(CHMOD) 700 $(DATA_DIR_FULL)
	$(SILENT) $(CHMOD) 700 $(DATA_DIR_FULL)/default_pref
	$(SILENT) $(CHMOD) 700 $(ATTACHMENT_DIR_FULL)
	$(SILENT) $(CHOWN) www:www $(DATA_DIR_FULL)
	$(SILENT) $(CHOWN) www:www $(DATA_DIR_FULL)/default_pref
	$(SILENT) $(CHOWN) www:www $(ATTACHMENT_DIR_FULL) 
	$(SILENT) $(CP) $(HTTPD_CONF_FILE) $(HTTPD_CONF_DST)
	$(SILENT) $(CP) $(HTTPD_CONF_FILE) $(HTTPD_CONF_DST)/$(HTTPD_DEFAULT_CONF_FILE)
	$(SILENT) $(CP) $(LOGO) $(IMAGES_DIR_FULL)
	$(SILENT) $(CP) $(SETUP_FILE) $(SETUP_DIR_FULL)
	$(INSTALL) -m 444 -o root -g wheel SquirrelMail.plist $(DSTROOT)$(VERSIONS_DIR)
	$(INSTALL) -m 444 -o root -g wheel SquirrelMail.txt $(DSTROOT)$(LICENSE_DIR)
	$(SILENT) $(CHMOD) 755 $(SETUP_DIR_FULL)/$(SETUP_FILE)
	$(SILENT) $(DITTO) locale $(LOCALE_DIR_FULL)
	$(SILENT) $(CHOWN) -R root:wheel $(LOCALE_DIR_FULL)
	$(SILENT) $(DITTO) help $(HELP_DIR_FULL)
	$(SILENT) $(CHOWN) -R root:wheel $(HELP_DIR_FULL)
	$(SILENT) $(DITTO) images $(IMAGES_DIR_FULL)
	$(SILENT) $(CHOWN) -R root:wheel $(IMAGES_DIR_FULL)
	$(SILENT) $(ECHO) "Install complete."

do_installhdrs:
	$(SILENT) $(ECHO) "No headers to install"

do_installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) $(PROJECT_FILES) $(SRCROOT)
	$(SILENT) $(CP) $(PROJECT_ARCHIVE) $(SRCROOT)

do_clean:
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) -$(RM) -rf $(PROJECT_DIR)

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(HTTPD_CONF_DST):
	$(SILENT) $(MKDIRS) $@

$(DATA_DIR_FULL):
	$(SILENT) $(MKDIRS) $@

$(ATTACHMENT_DIR_FULL):
	$(SILENT) $(MKDIRS) $@

$(CONFIG_DIR_FULL):
	$(SILENT) $(MKDIRS) $@

$(SHARE_DIR_FULL):
	$(SILENT) $(MKDIRS) $@

$(SETUP_DIR_FULL):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) $@

