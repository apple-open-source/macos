#
# xbs-compatible wrapper Makefile for cyrus imap
#

PROJECT=CyrusIMAP
VERSION=2.3.7

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

PROJECT_NAME=cyrus_imap
SW_VERS=`sw_vers | grep BuildVersion | sed 's/^.*://'`

OPEN_SRC_INFO_SRC_DIR=/CyrusIMAP.OpenSourceInfo
OPEN_SRC_VERS_DST_DIR=/usr/local/OpenSourceVersions
OPEN_SRC_LICS_DST_DIR=/usr/local/OpenSourceLicenses

SETUP_EXTRAS_SRC_DIR=CyrusIMAP.SetupExtras
SETUP_EXTRAS_DST_DIR=/System/Library/ServerSetup/SetupExtras
MIGRATION_EXTRAS_DST_DIR=/System/Library/ServerSetup/MigrationExtras

LAUNCHD_SRC_DIR=/CyrusIMAP.LaunchDaemons
LAUNCHD_DST_DIR=/System/Library/LaunchDaemons

CONFIG_SRC_DIR=/CyrusIMAP.Config
CONFIG_DST_DIR=/private/etc

BIN_DST_DIR=/usr/bin/cyrus/bin
TEST_DST_DIR=/usr/bin/cyrus/test
SIEVE_DST_DIR=/usr/bin/cyrus/sieve
TOOLS_DST_DIR=/usr/bin/cyrus/tools
ADMIN_DST_DIR=/usr/bin/cyrus/admin
MAN_DST_DIR=/usr/share/man

STRIP=/usr/bin/strip

LIB_PERL=/System/Library/Perl/
PERL_VER=`perl -V:version | sed -n -e "s/[^0-9.]*\([0-9.]*\).*/\1/p"`

CYRUS_TOOLS= dohash mkimap mupdate-loadgen.pl not-mkdep rehash \
			 translatesieve undohash upgradesieve
LOCAL_DIRS= System bin include lib share usr
CYRUS_BINS= arbitron chk_cyrus ctl_cyrusdb ctl_deliver ctl_mboxlist \
			cvt_cyrusdb cyr_expire cyrdump cyrus-notifyd cyrus-quota \
			deliver fud idled imapd ipurge lmtpd make_md5 master mbexamine \
			mbpath mupdate pop3d pop3proxyd proxyd reconstruct sievec smmapd \
			squatter sync_client sync_reset sync_server timsieved tls_prune \
			cyr_dbtool unexpunge

CYRUS_CONFIG = \
	--build=powerpc-apple-netbsd \
	--with-bdb-libdir=/usr/local/BerkeleyDB/lib \
	--with-bdb-incdir=/usr/local/BerkeleyDB/include \
	--with-sasl=/usr \
	--with-mboxlist-db=berkeley \
	--with-seen-db=skiplist \
	--with-subs-db=flat \
	--with-openssl=/usr \
	--with-auth=krb \
	--enable-gssapi \
	--disable-krb4 \
	--with-com_err \
	--with-extraident="OS X Server 10.5:$(SW_VERS)" \
	--enable-murder \
	--enable-replication \
	--enable-idled \
	--with-service-path=$(BIN_DST_DIR) \
	--with-pidfile=/var/run/cyrus-master.pid \
	--with-cyrus-user=_cyrus \
	--mandir=/usr/share/man \
	--without-snmp \
	BI_RC_CFLAGS="$(CFLAGS)"

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: make_imap

install :: make_imap_install clean_src strip_imap_binaries

install_debug :: make_imap_install

make_imap : configure_imap build_imap

make_imap_clean : clean_imap configure_imap build_imap

make_imap_install : configure_imap build_imap install_imap

make_imap_clean_install : clean_imap configure_imap build_imap install_imap

build_all : configure_imap build_imap

build_install : build_all install_imap

clean : clean_src

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

clean_src :
	$(SILENT) if [ -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make distclean)\
	fi

configure_imap :
	$(SILENT) $(ECHO) "-------------- Configuring $(PROJECT_NAME) --------------"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(CYRUS_CONFIG))\
	fi
	$(SILENT) $(ECHO) "---- Configuring $(PROJECT_NAME) complete."

clean_imap_src : 
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) $(ECHO) "-------------- Cleaning $(PROJECT_NAME) --------------"
	$(SILENT) if [ -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make distclean)\
	fi
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME) complete."

build_imap :
	$(SILENT) $(ECHO) "-------------- Building $(PROJECT_NAME) -------------- build_imap"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(CYRUS_CONFIG))\
	fi
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" depend)
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" all)
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" install)
	$(SILENT) $(ECHO) "---- Building $(PROJECT_NAME) complete."

install_imap : $(DSTROOT)$(LIB_PERL)
	$(SILENT) $(ECHO) "-------------- Installing $(PROJECT_NAME) --------------"
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME)..."

	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make install DESTDIR="$(DSTROOT)")
	$(SILENT) if [ -d "$(DSTROOT)/usr/local/lib/perl5/site_perl" ]; then\
		$(SILENT) ($(CD) "$(DSTROOT)/usr/local/lib/perl5/site_perl/" && $(CP) -rpf * "$(DSTROOT)$(LIB_PERL)$(PERL_VER)")\
	fi

	# Cyrus admin app
	$(SILENT) install -d -m 755 $(DSTROOT)/$(ADMIN_DST_DIR)
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/cyradm" "$(DSTROOT)/$(ADMIN_DST_DIR)/"

	# Install man pages
	$(SILENT) install -d -m 755 $(DSTROOT)/$(MAN_DST_DIR)
	$(SILENT) ($(CD) "$(DSTROOT)/usr/local/share/man" && $(CP) -r * "$(DSTROOT)/$(MAN_DST_DIR)/")

	# Renaming man pages
	$(SILENT) ($(MV) "$(DSTROOT)/$(MAN_DST_DIR)/man8/master.8" "$(DSTROOT)/$(MAN_DST_DIR)/man8/cyrus-master.8")
	$(SILENT) ($(MV) "$(DSTROOT)/$(BIN_DST_DIR)/notifyd" "$(DSTROOT)/$(BIN_DST_DIR)/cyrus-notifyd")
	$(SILENT) ($(MV) "$(DSTROOT)/$(MAN_DST_DIR)/man8/notifyd.8" "$(DSTROOT)/$(MAN_DST_DIR)/man8/cyrus-notifyd.8")
	$(SILENT) ($(MV) "$(DSTROOT)/$(BIN_DST_DIR)/quota" "$(DSTROOT)/$(BIN_DST_DIR)/cyrus-quota")
	$(SILENT) ($(MV) "$(DSTROOT)/$(MAN_DST_DIR)/man8/quota.8" "$(DSTROOT)/$(MAN_DST_DIR)/man8/cyrus-quota.8")

	# Install sieve apps
	$(SILENT) install -d -m 755 $(DSTROOT)/$(SIEVE_DST_DIR)
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/sieveshell" "$(DSTROOT)/$(SIEVE_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/installsieve" "$(DSTROOT)/$(SIEVE_DST_DIR)"

	# Install test apps
	$(SILENT) install -d -m 755 $(DSTROOT)/$(TEST_DST_DIR)
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/imtest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/lmtptest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/mupdatetest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/nntptest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/pop3test" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/sivtest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/smtptest" "$(DSTROOT)/$(TEST_DST_DIR)"
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/synctest" "$(DSTROOT)/$(TEST_DST_DIR)"

	# Creating tools directories
	$(SILENT) install -d -m 755 $(DSTROOT)/$(TOOLS_DST_DIR)
	$(SILENT) install -m 0755 "$(DSTROOT)/usr/local/bin/sieveshell" "$(DSTROOT)/$(TOOLS_DST_DIR)"
	for file in $(CYRUS_TOOLS); \
	do \
		$(SILENT) install -m 755 "$(SRCROOT)/$(PROJECT_NAME)/tools/$$file" "$(DSTROOT)$(TOOLS_DST_DIR)" ; \
	done

	# Setup & Migration Extras
	$(SILENT) install -d -m 0755 $(DSTROOT)/$(SETUP_EXTRAS_DST_DIR)
	$(SILENT) install -d -m 0755 $(DSTROOT)/$(MIGRATION_EXTRAS_DST_DIR)
	$(SILENT) install -m 0755 "$(SRCROOT)/$(SETUP_EXTRAS_SRC_DIR)/cyrus" "$(DSTROOT)/$(SETUP_EXTRAS_DST_DIR)"
	$(SILENT) install -m 0755 "$(SRCROOT)/$(SETUP_EXTRAS_SRC_DIR)/migrate_cyrus_db" "$(DSTROOT)/$(MIGRATION_EXTRAS_DST_DIR)/61_migrate_cyrus_db"
	$(SILENT) install -m 0755 "$(SRCROOT)/$(SETUP_EXTRAS_SRC_DIR)/upgrade_cyrus_opts" "$(DSTROOT)/$(MIGRATION_EXTRAS_DST_DIR)/62_upgrade_cyrus_opts"
	$(SILENT) install -m 0755 "$(SRCROOT)/$(SETUP_EXTRAS_SRC_DIR)/migrate_pan_db" "$(DSTROOT)/$(TOOLS_DST_DIR)/migrate_pan_db"

	# Config Files
	$(SILENT) install -d -m 755 $(DSTROOT)/$(CONFIG_DST_DIR)
	$(SILENT) install -m 0640 "$(SRCROOT)/$(CONFIG_SRC_DIR)/cyrus.conf.default" "$(DSTROOT)/$(CONFIG_DST_DIR)/cyrus.conf.default"
	$(SILENT) install -m 0640 "$(SRCROOT)/$(CONFIG_SRC_DIR)/imapd.conf.default" "$(DSTROOT)/$(CONFIG_DST_DIR)/imapd.conf.default"

	# Launchd plist
	$(SILENT) install -d -m 755 "$(DSTROOT)/$(LAUNCHD_DST_DIR)"
	$(SILENT) install -m 0444 "$(SRCROOT)/$(LAUNCHD_SRC_DIR)/edu.cmu.andrew.cyrus.master.plist" "$(DSTROOT)/$(LAUNCHD_DST_DIR)"

	# Open Source Info
	$(SILENT) install -d -m 755 "$(DSTROOT)/$(OPEN_SRC_VERS_DST_DIR)"
	$(SILENT) install -d -m 755 "$(DSTROOT)/$(OPEN_SRC_LICS_DST_DIR)"
	$(SILENT) install -m 0444 "$(SRCROOT)/$(OPEN_SRC_INFO_SRC_DIR)/CyrusIMAP.plist" "$(DSTROOT)/$(OPEN_SRC_VERS_DST_DIR)"
	$(SILENT) install -m 0444 "$(SRCROOT)/$(OPEN_SRC_INFO_SRC_DIR)/CyrusIMAP.txt" "$(DSTROOT)/$(OPEN_SRC_LICS_DST_DIR)"

	$(SILENT) $(ECHO) "---- Installing $(PROJECT_NAME) complete."

strip_imap_binaries:
	$(SILENT) ($(RM) -rf $(DSTROOT)/usr/bin/cyrus/include/cyrus/strhash.o)
	$(SILENT) ($(RM) -rf $(DSTROOT)/usr/bin/cyrus/lib)
	$(SILENT) ($(STRIP) -S $(DSTROOT)$(LIB_PERL)$(PERL_VER)/darwin-thread-multi-2level/auto/Cyrus/IMAP/IMAP.bundle)
	$(SILENT) ($(STRIP) -S $(DSTROOT)$(LIB_PERL)$(PERL_VER)/darwin-thread-multi-2level/auto/Cyrus/SIEVE/managesieve/managesieve.bundle)
	$(SILENT) (/bin/echo "$(PROJECT)$(VERSION)" >> $(DSTROOT)$(LIB_PERL)$(PERL_VER)/darwin-thread-multi-2level/auto/Cyrus/IMAP/IMAP.bs)
	$(SILENT) (/bin/echo "$(PROJECT)$(VERSION)" >> $(DSTROOT)$(LIB_PERL)$(PERL_VER)/darwin-thread-multi-2level/auto/Cyrus/SIEVE/managesieve/managesieve.bs)
	for local_dir in $(LOCAL_DIRS); \
	do \
		$(SILENT) if [ -d "$(DSTROOT)/usr/local/$$local_dir" ]; then \
			$(SILENT) ($(RM) -rf "$(DSTROOT)/usr/local/$$local_dir") \
		fi \
	done

	for cyrus_bin in $(CYRUS_BINS); \
	do \
		$(SILENT) if [ -e "$(DSTROOT)$(BIN_DST_DIR)/$$cyrus_bin" ]; then \
			$(SILENT) ($(STRIP) -S "$(DSTROOT)$(BIN_DST_DIR)/$$cyrus_bin") \
		fi \
	done
	$(SILENT) ($(STRIP) -S "$(DSTROOT)$(BIN_DST_DIR)/proxyd")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)$(BIN_DST_DIR)/pop3proxyd")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)$(BIN_DST_DIR)/lmtpproxyd")

.PHONY: clean installhdrs installsrc build install 

$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LIB_PERL) :
	$(SILENT) $(MKDIRS) $@
