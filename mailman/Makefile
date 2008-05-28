#
# xbs-compatible wrapper Makefile for mailman 
#

PROJECT=mailman
VERSION=2.1.9

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=
VARDIR=/private/var
SHAREDIR=/usr/share

# Configuration values we customize
#

SILENT=$(_v)
ECHO=echo
PROJECT_NAME=mailman
STRIP_FLAGS=-s
SO_STRIPFLAGS=-rx

HTTP_DIR=/usr/share/httpd/icons
SERVER_SETUP_DIR=/System/Library/ServerSetup/SetupExtras
SERVER_MIGRATION_DIR=/System/Library/ServerSetup/MigrationExtras
DOC_DIR=/Library/Documentation/Services/mailman
OPEN_SRC_INFO_DIR=/Mailman.OpenSourceInfo
SETUP_SRC_DIR=/Mailman.Setup

README_FILES=FAQ NEWS README README-I18N.en README.CONTRIB README.NETSCAPE README.USERAGENT

INSTALL_FLAGS = DESTDIR="$(DSTROOT)" BI_RC_CFLAGS="$(RC_CFLAGS)" OPT='-Os -mdynamic-no-pic' INSTALL='/usr/bin/install -g 78'
INSTALL_PREFIX = "$(SHAREDIR)/$(PROJECT)"

MAILMAN_CONFIG = \
	--prefix=/usr/share/mailman \
 	--localstatedir="$(VARDIR)/$(PROJECT)" \
	--with-var-prefix="$(VARDIR)/$(PROJECT)" \
	--with-mail-gid=_mailman \
	--with-cgi-gid=www \
	--without-permcheck
 
# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: build_mm

install :: configure_mm build_mm install-strip install-extras install-startup install-readmes install-group clean_src

clean : clean_src

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d "$(SRCROOT)/$(PROJECT)" ] && mkdir -p "$(SRCROOT)/$(PROJECT)"
	tar cf - . | (cd "$(SRCROOT)" ; tar xfp -)
	find "$(SRCROOT)" -type d -name CVS -print0 | xargs -0 rm -rf

clean_src :
	$(SILENT) if [ -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make distclean)\
	fi

configure_mm : 
	$(SILENT) $(ECHO) "--- Configuring $(PROJECT_NAME): Version: $(VERSION)"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(MAILMAN_CONFIG))\
	fi
	$(SILENT) $(ECHO) "--- Configuring $(PROJECT_NAME) complete."

build_mm :
	$(SILENT) $(ECHO) "--- Building $(PROJECT_NAME)"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(MAILMAN_CONFIG))\
	fi
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make $(INSTALL_FLAGS) )
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make $(INSTALL_FLAGS) install)
	$(SILENT) $(ECHO) "--- Building $(PROJECT_NAME) complete."

# Custom configuration:
#

install-strip :
	$(SILENT) $(ECHO) "---- Stripping language libraries..."
	$(SILENT) -$(STRIP) $(SO_STRIPFLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/japanese/c/_japanese_codecs.so \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/korean/c/_koco.so \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/korean/c/hangul.so
	$(SILENT) -$(STRIP) $(STRIPF_LAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/admin \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/admindb \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/confirm
	$(SILENT) -$(STRIP) $(STRIPF_LAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/create \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/edithtml \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/listinfo
	$(SILENT) -$(STRIP) $(STRIPF_LAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/options \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/private \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/rmlist
	$(SILENT) -$(STRIP) $(STRIPF_LAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/roster \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/subscribe \
		$(DSTROOT)$(INSTALL_PREFIX)/mail/mailman
	$(SILENT) $(ECHO) "---- Stripping language libraries complete."

install-extras : $(DSTROOT)$(HTTP_DIR) $(DSTROOT)$(SERVER_SETUP_DIR) $(DSTROOT)$(SERVER_MIGRATION_DIR)
	$(SILENT) $(ECHO) "---- Installing extras..."
	$(SILENT) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	$(SILENT) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	$(SILENT) $(CP) -p "$(DSTROOT)$(INSTALL_PREFIX)"/icons/* "$(DSTROOT)$(HTTP_DIR)"
	$(CHGRP) -R wheel "$(DSTROOT)$(HTTP_DIR)"
	$(SILENT) $(CP) $(SRCROOT)/$(SETUP_SRC_DIR)/SetupScript "$(DSTROOT)$(SERVER_SETUP_DIR)/Mailman"
	$(SILENT) $(CP) $(SRCROOT)/$(SETUP_SRC_DIR)/SetupScript "$(DSTROOT)$(SERVER_MIGRATION_DIR)/Mailman"
	$(CHGRP) -R wheel "$(DSTROOT)$(SERVER_SETUP_DIR)/Mailman"
	$(CHMOD) 755  "$(DSTROOT)$(SERVER_SETUP_DIR)/Mailman"
	$(SILENT) $(ECHO) "MTA = 'Postfix'" >> "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py.dist"
	$(RM) "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py"
	$(SILENT) $(ECHO) "---- Installing extras complete."

install-readmes : $(DSTROOT)$(DOC_DIR)
	$(SILENT) $(ECHO) "---- Installing Read Me files..."
	for file in $(README_FILES); \
	do \
		$(SILENT) $(CP) "$(SRCROOT)/$(PROJECT)/$$file" "$(DSTROOT)$(DOC_DIR)"; \
	done
	$(SILENT) $(ECHO) "---- Installing Read Me files complete."

install-startup :
	$(SILENT) $(ECHO) "---- Installing Startup Item..."
	$(SILENT) install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	$(SILENT) install -m 0644 $(SRCROOT)/Mailman.LaunchDaemons/org.list.mailmanctl.plist \
				  $(DSTROOT)/System/Library/LaunchDaemons/org.list.mailmanctl.plist
	$(SILENT) $(ECHO) "--- Installing Startup Item complete."

install-group :
	$(SILENT) $(ECHO) "---- Setting file permissions..."
	$(CHOWN) -R root:mailman "$(DSTROOT)/usr/share/mailman"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/archives"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/data"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/lists"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/locks"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/logs"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/qfiles"
	$(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/spam"
	$(SILENT) $(CD) "$(DSTROOT)/usr/share/mailman/templates" && /bin/ls | xargs chmod 02775
	$(SILENT) $(CD) "$(DSTROOT)/usr/share/mailman/messages" && /bin/ls | xargs chmod 02775
	$(SILENT) chmod 02775 "$(DSTROOT)/private/var/mailman"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/email"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/aliases"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/c"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/mappings"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/python"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/c"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/mappings"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/python"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.5"
	$(SILENT) chmod 02775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.5/site-packages"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/admin"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/admindb"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/confirm"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/create"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/edithtml"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/listinfo"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/options"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/private"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/rmlist"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/roster"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/subscribe"
	$(SILENT) chmod 02755 "$(DSTROOT)/usr/share/mailman/mail/mailman"
	$(SILENT) $(ECHO) "---- Setting file permissions complete."
	
.PHONY: clean installhdrs installsrc build install 

$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(HTTP_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SERVER_SETUP_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SERVER_MIGRATION_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(DOC_DIR) :
	$(SILENT) $(MKDIRS) $@
