# xbs-compatible wrapper Makefile for Clam AV
#

PROJECT=clamav

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=
CFLAGS=-O0 $(RC_CFLAGS)

# Configuration values we customize
#

PROJECT_NAME=clamav
OS_VER=0.95.2
CLAMAV_TAR_GZ=clamav-$(OS_VER).tar.gz
CLAMAV_DIFF_5876278=clamav-$(OS_VER)-5876278.diff

CLAMAV_BUILD_DIR=/clamav/clamav-$(OS_VER)
ETC_DIR=/private/etc
VAR_CLAM=/private/var/clamav
CLAM_SHARE_DIR=/private/var/clamav/share
CLAM_STATE_DIR=/private/var/clamav/state
LIB_TOOL=$(SRCROOT)/$(CLAMAV_BUILD_DIR)/libtool
LAUNCHDDIR=/System/Library/LaunchDaemons

TEMP_DIR=/Temp_Dir

BINARY_DIR=clamav.Bin
CONFIG_DIR=clamav.Conf
OS_SRC_DIR=clamav.OpenSourceInfo
LD_SRC_DIR=clamav.LaunchDaemons

USR=/usr
USR_BIN=/usr/bin
USR_SBIN=/usr/sbin
SHARE_1_DIR=/usr/share/man/man1
SHARE_5_DIR=/usr/share/man/man5
SHARE_8_DIR=/usr/share/man/man8
USR_LOCAL=/usr/local
USR_OS_VERSION=$(USR_LOCAL)/OpenSourceVersions
USR_OS_LICENSE=$(USR_LOCAL)/OpenSourceLicenses

SETUP_EXTRAS_SRC_DIR=clamav.SetupExtras
COMMON_EXTRAS_DST_DIR=/System/Library/ServerSetup/CommonExtras

STRIP=/usr/bin/strip
GNUTAR=/usr/bin/gnutar
CHOWN=/usr/sbin/chown
PATCH=/usr/bin/patch

# Clam Antivirus config
#

CLAMAV_CONFIG= \
	--prefix=/ \
	--exec-prefix=/usr \
	--bindir=/usr/bin \
	--sbindir=/usr/sbin \
	--libexecdir=/usr/libexec \
	--datadir=/usr/share/clamav \
	--sysconfdir=/private/etc \
	--sharedstatedir=/private/var/clamav/share \
	--localstatedir=/private/var/clamav/state \
	--disable-dependency-tracking \
	--libdir=/usr/lib \
	--includedir=/usr/share/clamav/include \
	--oldincludedir=/usr/share/clamav/include \
	--infodir=/usr/share/clamav/info \
	--mandir=/usr/share/man \
	--with-dbdir=/private/var/clamav \
	--disable-shared \
	--with-user=_clamav \
	--with-group=_clamav \
	--with-gnu-ld \
	--enable-static

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: make_clamav

install :: make_clamav_install

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

make_clamav :
	$(SILENT) $(ECHO) "------------ Make Clam AV ------------"
	$(SILENT) if [ -e "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_TAR_GZ)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT)" && $(GNUTAR) -xzpf "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_TAR_GZ)") ; \
	fi
	$(SILENT) if [ -e "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_DIFF_5876278)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && $(PATCH) -p1 < "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_DIFF_5876278)") ; \
	fi
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && ./configure $(CLAMAV_CONFIG))
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && make CFLAGS="$(CFLAGS)")

make_clamav_install :
	# Unstuff archive
	$(SILENT) $(ECHO) "------------ Make Install Clam AV ------------"
	$(SILENT) if [ -e "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_TAR_GZ)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT)" && $(GNUTAR) -xzpf "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_TAR_GZ)") ; \
	fi
	$(SILENT) if [ -e "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_DIFF_5876278)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && $(PATCH) -p1 < "$(SRCROOT)/$(BINARY_DIR)/$(CLAMAV_DIFF_5876278)") ; \
	fi


	# Configure and make Clam AV
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && ./configure $(CLAMAV_CONFIG))
	if grep -qs 'LTCFLAGS=\"-g -O2\"' $(SRCROOT)/$(CLAMAV_BUILD_DIR)/libtool ; then \
		mv $(LIB_TOOL) $(LIB_TOOL).bak ; \
		sed -e 's/LTCFLAGS=\"-g -O2\"/LTCFLAGS=\"$(CFLAGS)"/g' $(LIB_TOOL).bak > $(LIB_TOOL) ; \
	fi
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && make CFLAGS="$(CFLAGS)")
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && make "DESTDIR=$(SRCROOT)/$(TEMP_DIR)" CFLAGS="$(CFLAGS)" install)
	install -m 0755 "$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/UpgradeClamAV" \
			"$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/66_clamav_migrator"
	$(SILENT) ($(RM) -rf "$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/UpgradeClamAV")

	# Create install directories
	install -d -m 0755 "$(DSTROOT)$(ETC_DIR)"
	install -d -m 0755 "$(DSTROOT)$(CLAM_SHARE_DIR)"
	install -d -m 0755 "$(DSTROOT)$(CLAM_STATE_DIR)"
	install -d -m 0755 "$(DSTROOT)$(LAUNCHDDIR)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_VERSION)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_LICENSE)"
	install -d -m 0755 "$(DSTROOT)$(COMMON_EXTRAS_DST_DIR)"

	# Install defautl config files
	install -m 0644 "$(SRCROOT)/$(CONFIG_DIR)/clamd.conf" "$(DSTROOT)$(ETC_DIR)/clamd.conf"
	install -m 0644 "$(SRCROOT)/$(CONFIG_DIR)/clamd.conf" "$(DSTROOT)$(ETC_DIR)/clamd.conf.default"
	install -m 0644 "$(SRCROOT)/$(CONFIG_DIR)/freshclam.conf" "$(DSTROOT)$(ETC_DIR)/freshclam.conf"
	install -m 0644 "$(SRCROOT)/$(CONFIG_DIR)/freshclam.conf" "$(DSTROOT)$(ETC_DIR)/freshclam.conf.default"

	# Install & strip binaries
	install -d -m 0755 "$(DSTROOT)$(USR_BIN)"
	#install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_BIN)/clamav-config" "$(DSTROOT)$(USR_BIN)/clamav-config"
	install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_BIN)/clamdscan" "$(DSTROOT)$(USR_BIN)/clamdscan"
	install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_BIN)/clamscan" "$(DSTROOT)$(USR_BIN)/clamscan"
	install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_BIN)/freshclam" "$(DSTROOT)$(USR_BIN)/freshclam"
	install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_BIN)/sigtool" "$(DSTROOT)$(USR_BIN)/sigtool"

	install -d -m 0755 "$(DSTROOT)$(USR_SBIN)"
	install -m 0755 -s "$(SRCROOT)$(TEMP_DIR)$(USR_SBIN)/clamd" "$(DSTROOT)$(USR_SBIN)/clamd"

	# Install man pages
	install -d -m 0755 "$(DSTROOT)$(SHARE_1_DIR)"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_1_DIR)/clamdscan.1" "$(DSTROOT)$(SHARE_1_DIR)/clamdscan.1"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_1_DIR)/clamscan.1" "$(DSTROOT)$(SHARE_1_DIR)/clamscan.1"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_1_DIR)/freshclam.1" "$(DSTROOT)$(SHARE_1_DIR)/freshclam.1"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_1_DIR)/sigtool.1" "$(DSTROOT)$(SHARE_1_DIR)/sigtool.1"
	#install -m 0444 "$(SRCROOT)/$(CONFIG_DIR)/clamav-config.1" "$(DSTROOT)$(SHARE_1_DIR)/clamav-config.1"

	install -d -m 0755 "$(DSTROOT)$(SHARE_5_DIR)"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_5_DIR)/clamd.conf.5" "$(DSTROOT)$(SHARE_5_DIR)/clamd.conf.5"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_5_DIR)/freshclam.conf.5" "$(DSTROOT)$(SHARE_5_DIR)/freshclam.conf.5"

	install -d -m 0755 "$(DSTROOT)$(SHARE_8_DIR)"
	#install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_8_DIR)/clamav-milter.8" "$(DSTROOT)$(SHARE_8_DIR)/clamav-milter.8"
	install -m 0444 "$(SRCROOT)$(TEMP_DIR)$(SHARE_8_DIR)/clamd.8" "$(DSTROOT)$(SHARE_8_DIR)/clamd.8"

	# Install default clam databases
	install -d -m 0755 "$(DSTROOT)$(VAR_CLAM)"
	install -m 644 "$(SRCROOT)$(TEMP_DIR)$(VAR_CLAM)/daily.cvd" "$(DSTROOT)$(VAR_CLAM)/daily.cvd"
	install -m 644 "$(SRCROOT)$(TEMP_DIR)$(VAR_CLAM)/main.cvd" "$(DSTROOT)$(VAR_CLAM)/main.cvd"
	chown -R 82 "$(DSTROOT)$(VAR_CLAM)"

	# Install Setup Extras
	install -m 0755 "$(SRCROOT)/$(SETUP_EXTRAS_SRC_DIR)/clamav" "$(DSTROOT)$(COMMON_EXTRAS_DST_DIR)/SetupClamAV.sh"
	install -m 0644 "$(SRCROOT)/$(LD_SRC_DIR)/org.clamav.clamd.plist" "$(DSTROOT)/$(LAUNCHDDIR)/org.clamav.clamd.plist"
	install -m 0644 "$(SRCROOT)/$(LD_SRC_DIR)/org.clamav.freshclam.plist" "$(DSTROOT)/$(LAUNCHDDIR)/org.clamav.freshclam.plist"

	# Install Open Source plist & License files
	install -m 444 "$(SRCROOT)/$(OS_SRC_DIR)/clamav.plist" "$(DSTROOT)/$(USR_OS_VERSION)/clamav.plist"
	install -m 444 "$(SRCROOT)/$(OS_SRC_DIR)/clamav.txt" "$(DSTROOT)/$(USR_OS_LICENSE)/clamav.txt"

	# Set ownership of installed directories & files
	$(SILENT) ($(CHOWN) -R root:wheel "$(DSTROOT)")
	#$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(ETC_DIR)")
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(VAR_CLAM)")
	$(SILENT) ($(CHOWN) -R root:wheel "$(DSTROOT)/usr/share/man")
	$(SILENT) ($(CHOWN) -R root:wheel "$(DSTROOT)/usr/bin")

	$(SILENT) ($(RM) -rf "$(SRCROOT)/$(TEMP_DIR)")
	$(SILENT) ($(RM) -rf "$(SRCROOT)/$(CLAMAV_BUILD_DIR)")

	$(SILENT) $(ECHO) "---- Building Clam AV complete."

.PHONY: installhdrs installsrc build install 

