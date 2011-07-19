#
# xbs-compatible wrapper Makefile for mailman 
#

PROJECT=mailman
VERSION=2.1.14

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
#

RC_ARCHS=
VARDIR=/private/var
SHAREDIR=/usr/share

# Configuration values we customize
#

SRC_DIR=$(PROJECT)-$(VERSION)
TAR_FILE=$(PROJECT)-$(VERSION).tgz
STRIP_FLAGS=-S

HTTP_DIR=/usr/share/httpd/icons
COMMON_DIR=/System/Library/ServerSetup/CommonExtras
DOC_DIR=/Library/Documentation/Services/mailman
OPEN_SRC_INFO_DIR=/Mailman.OpenSourceInfo
SETUP_SRC_DIR=/Mailman.Setup
BIN_DIR=/Mailman.Bin
PATCH_DIR=/Mailman.Patch
EXTRAS=/Mailman.Extras
BUILD_DIR=/Build
HTTPD2_CONF_DIR=$(shell /usr/sbin/apxs -q SYSCONFDIR)
WEBAPP_DIR=$(HTTPD2_CONF_DIR)/webapps

DSYMUTIL=/usr/bin/dsymutil

README_FILES=FAQ NEWS README README-I18N.en README.CONTRIB README.NETSCAPE README.USERAGENT

INSTALL_FLAGS = DESTDIR="$(DSTROOT)" BI_RC_CFLAGS="$(RC_CFLAGS)" OPT='-mdynamic-no-pic' INSTALL='/usr/bin/install -g 78' DIRSETGID=:
INSTALL_PREFIX = "$(SHAREDIR)/$(PROJECT)"

MAILMAN_CONFIG = \
	--prefix=/usr/share/mailman \
	--localstatedir="$(VARDIR)/$(PROJECT)" \
	--with-var-prefix="$(VARDIR)/$(PROJECT)" \
	--with-mail-gid=_mailman \
	--with-cgi-gid=_www \
	--without-permcheck

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: build_mm

install :: patch_mm configure_mm build_mm install-strip install-extras install-startup install-readmes install-group clean_up

clean : clean_src

installhdrs :
	@echo "No headers to install"

installsrc :
	[ ! -d "$(SRCROOT)/$(PROJECT)" ] && mkdir -p "$(SRCROOT)/$(PROJECT)"
	tar cf - . | (cd "$(SRCROOT)" ; tar xfp -)
	find "$(SRCROOT)" -type d -name CVS -print0 | xargs -0 rm -rf

clean_src :
	$(_v) if [ -e "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)/Makefile" ]; then\
		$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && make distclean)\
	fi

clean_up :
	$(_v) if [ -d "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" ]; then\
		$(_v) ($(RM) -rf "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)")\
	fi

patch_mm : $(OBJROOT)$(BUILD_DIR)
	$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)" && \
		/usr/bin/gnutar -xzvpf "$(SRCROOT)/$(BIN_DIR)/$(TAR_FILE)")
	$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && \
		/usr/bin/patch -p1 < "$(SRCROOT)/$(PATCH_DIR)/CVE-2011-0707.diff")
	$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && \
		/usr/bin/patch -p1 < "$(SRCROOT)/$(PATCH_DIR)/apple-mods.diff")

configure_mm : 
	@echo "--- Configuring $(PROJECT): Version: $(VERSION)"
	@echo "Configuring $(PROJECT)..."
	$(_v) if [ ! -e "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)/Makefile" ]; then\
		$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && ./configure $(MAILMAN_CONFIG))\
	fi
	@echo "--- Configuring $(PROJECT) complete ---"

build_mm :
	@echo "--- Building $(PROJECT)"
	@echo "Configuring $(PROJECT)..."
	$(_v) if [ ! -e "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)/Makefile" ]; then\
		$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && ./configure $(MAILMAN_CONFIG))\
	fi
	$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && make $(INSTALL_FLAGS) )
	$(_v) ($(CD) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)" && make $(INSTALL_FLAGS) install)
	@echo "--- Building $(PROJECT) complete ---"

# Custom configuration:
#

install-strip :
	@echo "---- Stripping binaries..."
	$(_v) for file in $(DSTROOT)$(INSTALL_PREFIX)/cgi-bin/*;\
	do \
		echo "Processing $${file##*/} (from $${file})";	\
		if [ ! -e "$(SYMROOT)/$${file##*/}" ]; then	\
			echo "  $(CP) $${file} $(SYMROOT)";	\
			$(CP) $${file} $(SYMROOT);	\
		fi;	\
		if [ -e "$(SYMROOT)/$${file##*/}.dSYM" ]; then	\
			echo "...odd, dSYM already exists.";	\
		else	\
			echo "  $(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file}";\
			$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		fi;	\
		$(STRIP) -S $${file};	\
	done
	$(_v) for file in $(DSTROOT)$(INSTALL_PREFIX)/mail/*;\
	do \
		echo "Processing $${file##*/} (from $${file})";	\
		if [ ! -e "$(SYMROOT)/$${file##*/}" ]; then	\
			echo "  $(CP) $${file} $(SYMROOT)";	\
			$(CP) $${file} $(SYMROOT);	\
		fi;	\
		if [ -e "$(SYMROOT)/$${file##*/}.dSYM" ]; then	\
			echo "...odd, dSYM already exists.";	\
		else	\
			echo "  $(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file}";\
			$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		fi;	\
		$(STRIP) -S $${file};	\
	done
	@echo "---- Stripping binaries complete..."

install-extras : $(DSTROOT)$(HTTP_DIR) $(DSTROOT)$(COMMON_DIR)
	@echo "---- Installing extras..."
	$(_v) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	$(_v) install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	$(_v) install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.plist $(DSTROOT)/usr/local/OpenSourceVersions
	$(_v) install -m 0444 $(SRCROOT)/$(OPEN_SRC_INFO_DIR)/mailman.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	$(_v) install -m 0755 $(SRCROOT)/$(EXTRAS)/get_list_info $(DSTROOT)/usr/share/mailman/bin/get_list_info
	$(_v) install -m 0755 $(SRCROOT)/$(EXTRAS)/apple_config_list $(DSTROOT)/usr/share/mailman/bin/apple_config_list
	$(_v) $(CP) -p "$(DSTROOT)$(INSTALL_PREFIX)"/icons/* "$(DSTROOT)$(HTTP_DIR)"
	$(CHGRP) -R wheel "$(DSTROOT)$(HTTP_DIR)"
	$(_v) install -d -m 755 $(DSTROOT)$(WEBAPP_DIR)
	$(_v) install -m 0444 $(SRCROOT)/$(EXTRAS)/com.apple.webapp.mailman.plist $(DSTROOT)$(WEBAPP_DIR)
	$(_v) install -m 0644 $(SRCROOT)/$(EXTRAS)/httpd_mailman.conf $(DSTROOT)$(HTTPD2_CONF_DIR)
	$(_v) install -m 0444 $(SRCROOT)/$(EXTRAS)/httpd_mailman.conf $(DSTROOT)$(HTTPD2_CONF_DIR)/httpd_mailman.conf.default
	$(_v) install -m 0755 "$(SRCROOT)/$(SETUP_SRC_DIR)/SetupScript" "$(DSTROOT)$(COMMON_DIR)/SetupMailman.sh"
	@echo "MTA = 'Postfix'" >> "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py.dist"
	$(_v) $(RM) "$(DSTROOT)$(INSTALL_PREFIX)/Mailman/mm_cfg.py"
	@echo "---- Installing extras complete ---"

install-readmes : $(DSTROOT)$(DOC_DIR)
	@echo "---- Installing Read Me files..."
	$(_v) for file in $(README_FILES); \
	do \
		$(_v) $(CP) "$(OBJROOT)$(BUILD_DIR)/$(SRC_DIR)/$$file" "$(DSTROOT)$(DOC_DIR)"; \
	done
	@echo "---- Installing Read Me files complete ---"

install-startup :
	@echo "---- Installing Startup Item..."
	$(_v) install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	$(_v) install -m 0644 $(SRCROOT)/Mailman.LaunchDaemons/org.list.mailmanctl.plist \
				  $(DSTROOT)/System/Library/LaunchDaemons/org.list.mailmanctl.plist
	@echo "--- Installing Startup Item complete ---"

install-group :
	@echo "---- Setting file permissions..."
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/usr/share/mailman"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/archives"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/data"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/lists"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/locks"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/logs"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/qfiles"
	$(_v) $(CHOWN) -R root:mailman "$(DSTROOT)/private/var/mailman/spam"
	$(_v) $(CD) "$(DSTROOT)/usr/share/mailman/templates" && /bin/ls | xargs chmod 0775
	$(_v) $(CD) "$(DSTROOT)/usr/share/mailman/messages" && /bin/ls | xargs chmod 0775
	$(_v) chmod 0775 "$(DSTROOT)/private/var/mailman"
	$(_v) chmod 0775 "$(DSTROOT)/usr/share/mailman"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/admin"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/admindb"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/confirm"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/create"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/edithtml"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/listinfo"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/options"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/private"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/rmlist"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/roster"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/cgi-bin/subscribe"
	$(_v) chmod 02755 "$(DSTROOT)/usr/share/mailman/mail/mailman"
	$(_v) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/"*
	$(_v) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/gl/LC_MESSAGES"
	$(_v) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/he/LC_MESSAGES"
	$(_v) chmod 0755 "$(DSTROOT)/usr/share/mailman/messages/sk/LC_MESSAGES"
	$(_v) chmod 0755 "$(DSTROOT)/usr/share/mailman/templates/"*
	@echo "---- Setting file permissions complete ---"
	
.PHONY: clean installhdrs installsrc build install 

$(OBJROOT)$(BUILD_DIR) :
	$(_v) $(MKDIRS) $@

$(DSTROOT) :
	$(_v) $(MKDIRS) $@

$(DSTROOT)$(HTTP_DIR) :
	$(_v) $(MKDIRS) $@

$(DSTROOT)$(COMMON_DIR) :
	$(_v) $(MKDIRS) $@

$(DSTROOT)$(DOC_DIR) :
	$(_v) $(MKDIRS) $@
