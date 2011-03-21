#
# xbs-compatible wrapper Makefile for mailman 
#

PROJECT=mailman
VERSION=2.1.14

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
STRIP_FLAGS=-S
SO_STRIP_FLAGS=-rx

BUILD_DIR=$(OBJROOT)/build
MM_BUILD_DIR=/$(BUILD_DIR)/$(PROJECT)-$(VERSION)
MM_TAR_GZ=$(PROJECT)-$(VERSION).tgz
HTTP_DIR=/usr/share/httpd/icons
COMMON_DIR=/System/Library/ServerSetup/CommonExtras
DOC_DIR=/Library/Documentation/Services/mailman
OPEN_SRC_INFO_DIR=/Mailman.OpenSourceInfo
SETUP_SRC_DIR=/Mailman.Setup
PATCH_DIR=/Mailman.Patch
BINARY_DIR=/Mailman.Bin

# binaries
GNUTAR=/usr/bin/gnutar

README_FILES=FAQ NEWS README README-I18N.en README.CONTRIB README.NETSCAPE README.USERAGENT

INSTALL_FLAGS = DESTDIR="$(DSTROOT)" BI_RC_CFLAGS="$(RC_CFLAGS)" OPT='-Os -mdynamic-no-pic' INSTALL='/usr/bin/install -g 78' DIRSETGID=:
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

install :: patch_mm configure_mm build_mm install-strip install-extras install-startup install-readmes install-group clean_src

clean : clean_src

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d "$(SRCROOT)/$(PROJECT)" ] && mkdir -p "$(SRCROOT)/$(PROJECT)"
	tar cf - . | (cd "$(SRCROOT)" ; tar xfp -)
	find "$(SRCROOT)" -type d -name CVS -print0 | xargs -0 rm -rf

clean_src :
	$(SILENT) if [ -e "$(MM_BUILD_DIR)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && make distclean)\
	fi

patch_mm :
	$(SILENT) if [ ! -d "$(BUILD_DIR)" ]; then \
		$(SILENT) (mkdir "$(BUILD_DIR)"); \
	fi
	$(SILENT) ($(CD) "$(BUILD_DIR)" && $(GNUTAR) -xzpf $(SRCROOT)$(BINARY_DIR)/$(MM_TAR_GZ) )
	$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && /usr/bin/patch -p1 < "$(SRCROOT)/$(PATCH_DIR)/apple-mods.diff")

configure_mm : 
	$(SILENT) $(ECHO) "--- Configuring $(PROJECT_NAME): Version: $(VERSION)"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(MM_BUILD_DIR)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && ./configure $(MAILMAN_CONFIG))\
	fi
	$(SILENT) $(ECHO) "--- Configuring $(PROJECT_NAME) complete."

build_mm :
	$(SILENT) $(ECHO) "--- Building $(PROJECT_NAME)"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(MM_BUILD_DIR)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && ./configure $(MAILMAN_CONFIG))\
	fi
	$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && make $(INSTALL_FLAGS) )
	$(SILENT) ($(CD) "$(MM_BUILD_DIR)" && make $(INSTALL_FLAGS) install)
	$(SILENT) $(ECHO) "--- Building $(PROJECT_NAME) complete."

# Custom configuration:
#

install-strip :
	$(SILENT) $(ECHO) "---- Stripping language libraries..."
	$(SILENT) -$(STRIP) $(SO_STRIP_FLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/japanese/c/_japanese_codecs.so \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/korean/c/_koco.so \
		$(DSTROOT)$(INSTALL_PREFIX)/pythonlib/korean/c/hangul.so
	$(SILENT) -$(STRIP) $(STRIP_FLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/admin \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/admindb \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/confirm
	$(SILENT) -$(STRIP) $(STRIP_FLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/create \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/edithtml \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/listinfo
	$(SILENT) -$(STRIP) $(STRIP_FLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/options \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/private \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/rmlist
	$(SILENT) -$(STRIP) $(STRIP_FLAGS) \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/roster \
		$(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/subscribe \
		$(DSTROOT)$(INSTALL_PREFIX)/mail/mailman
	$(SILENT) $(ECHO) "---- Stripping language libraries complete."

install-extras : $(DSTROOT)$(HTTP_DIR) $(DSTROOT)$(COMMON_DIR)
	$(SILENT) $(ECHO) "---- Installing extras..."
	$(SILENT) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	$(SILENT) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	$(SILENT) $(CP) -p "$(DSTROOT)$(INSTALL_PREFIX)"/icons/* "$(DSTROOT)$(HTTP_DIR)"
	$(CHGRP) -R wheel "$(DSTROOT)$(HTTP_DIR)"
	install -m 0755 "$(SRCROOT)/$(SETUP_SRC_DIR)/SetupScript" "$(DSTROOT)$(COMMON_DIR)/SetupMailman.sh"
	$(SILENT) $(ECHO) "MTA = 'Postfix'" >> "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py.dist"
	$(RM) "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py"
	$(SILENT) $(ECHO) "---- Installing extras complete."

install-readmes : $(DSTROOT)$(DOC_DIR)
	$(SILENT) $(ECHO) "---- Installing Read Me files..."
	for file in $(README_FILES); \
	do \
		$(SILENT) $(CP) "$(MM_BUILD_DIR)/$$file" "$(DSTROOT)$(DOC_DIR)"; \
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
	$(SILENT) $(CD) "$(DSTROOT)/usr/share/mailman/templates" && /bin/ls | xargs chmod 0775
	$(SILENT) $(CD) "$(DSTROOT)/usr/share/mailman/messages" && /bin/ls | xargs chmod 0775
	$(SILENT) chmod 0775 "$(DSTROOT)/private/var/mailman"
	$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/aliases"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/c"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/mappings"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/japanese/python"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/c"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/mappings"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/korean/python"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.6"
	#$(SILENT) chmod 0775 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.6/site-packages"
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
	$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/"*
	$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/gl/LC_MESSAGES"
	$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/he/LC_MESSAGES"
	$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/sk/LC_MESSAGES"
	$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/templates/"*
	#$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.6"
	#$(SILENT) chmod 0755 "$(DSTROOT)/usr/share/mailman/pythonlib/lib/python2.6/site-packages"
	$(SILENT) $(ECHO) "---- Setting file permissions complete."
	
.PHONY: clean installhdrs installsrc build install 

$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(HTTP_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(COMMON_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(DOC_DIR) :
	$(SILENT) $(MKDIRS) $@
