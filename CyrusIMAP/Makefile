#
# xbs-compatible wrapper Makefile for cyrus imap
#

PROJECT=cyrus
VERSION=2.2.10

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

BIN_DIR=/usr/bin/cyrus/bin
TOOLSDIR=/usr/bin/cyrus/tools
TESTDIR=/usr/bin/cyrus/test
ADMINDIR=/usr/bin/cyrus/admin
ETCDIR=/private/etc
PERL_DIR=/System/Library
SHAREDIR=/usr/share/man
SETUPEXTRASDIR=SetupExtras
SASCRIPTSDIR=/System/Library/ServerSetup/SetupExtras
OSV_DIR=/usr/local/OpenSourceVersions
OSL_DIR=/usr/local/OpenSourceLicenses


STRIP=/usr/bin/strip

LIB_PERL=/System/Library/Perl/
PERL_VER=`perl -V:version | sed -n -e "s/[^0-9.]*\([0-9.]*\).*/\1/p"`

CYRUS_TOOLS= dohash mkimap mupdate-loadgen.pl not-mkdep rehash \
			 translatesieve undohash upgradesieve
LOCAL_DIRS= System bin include lib man usr

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
	--with-snmp=/usr/share/snmp \
	--with-extraident="OS X 10.3" \
	--enable-murder \
	--with-service-path=$(BIN_DIR) \
	--with-pidfile=/var/run/cyrus-master.pid \
	--with-cyrus-user=cyrus \
	--without-snmp \
	--mandir=/usr/share/man \
	BI_RC_CFLAGS="$(RC_CFLAGS)"

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
	$(SILENT) if [ -d "$(SRCROOT)/$(EXPORT_TOOL_NAME)/build" ]; then\
		$(SILENT) ($(RM) -r "$(SRCROOT)/$(EXPORT_TOOL_NAME)/build")\
	fi

configure_imap :
	$(SILENT) $(ECHO) "-------------- $(PROJECT_NAME) -------------- configure_imap"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(CYRUS_CONFIG))\
	fi
	$(SILENT) $(ECHO) "---- Configuring $(PROJECT_NAME) complete."

clean_imap_src : 
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) $(ECHO) "-------------- $(PROJECT_NAME) --------------"
	$(SILENT) if [ -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make distclean)\
	fi
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME) complete."

build_imap :
	$(SILENT) $(ECHO) "-------------- $(PROJECT_NAME) -------------- build_imap"
	$(SILENT) $(ECHO) "Configuring $(PROJECT_NAME)..."
	$(SILENT) if [ ! -e "$(SRCROOT)/$(PROJECT_NAME)/Makefile" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && ./configure $(CYRUS_CONFIG))\
	fi
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" depend)
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" all)
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make DESTDIR="$(DSTROOT)" install)
	$(SILENT) $(ECHO) "---- Building $(PROJECT_NAME) complete."

install_imap :  $(DSTROOT)$(TOOLSDIR) $(DSTROOT)$(TESTDIR) $(DSTROOT)$(ADMINDIR) $(DSTROOT)$(ETCDIR) \
		$(DSTROOT)$(SHAREDIR) $(DSTROOT)$(SASCRIPTSDIR) $(DSTROOT)$(OSV_DIR) \
		$(DSTROOT)$(OSL_DIR) $(DSTROOT)$(PERL_DIR)
	$(SILENT) $(ECHO) "-------------- $(PROJECT_NAME) --------------"
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME)..."
	$(SILENT) ($(CD) "$(SRCROOT)/$(PROJECT_NAME)" && make install DESTDIR="$(DSTROOT)")
	$(SILENT) ($(MV) "$(DSTROOT)/usr/local/Library/Perl" "$(DSTROOT)$(PERL_DIR)/")
	$(SILENT) ($(RM) -rf "$(DSTROOT)/usr/local/Library")
	$(SILENT) ($(CP) -rpf "$(DSTROOT)/usr/local/usr/bin/cyradm" "$(DSTROOT)$(ADMINDIR)/")
	$(SILENT) ($(CD) "$(DSTROOT)/usr/local/man" && $(CP) -r * "$(DSTROOT)/$(SHAREDIR)/")
	$(SILENT) ($(MV) "$(DSTROOT)/$(SHAREDIR)/man8/master.8" "$(DSTROOT)/$(SHAREDIR)/man8/cyrus-master.8")
	$(SILENT) ($(MV) "$(DSTROOT)/$(BIN_DIR)/notifyd" "$(DSTROOT)/$(BIN_DIR)/cyrus-notifyd")
	$(SILENT) ($(MV) "$(DSTROOT)/$(SHAREDIR)/man8/notifyd.8" "$(DSTROOT)/$(SHAREDIR)/man8/cyrus-notifyd.8")
	$(SILENT) ($(MV) "$(DSTROOT)/$(BIN_DIR)/quota" "$(DSTROOT)/$(BIN_DIR)/cyrus-quota")
	$(SILENT) ($(MV) "$(DSTROOT)/$(SHAREDIR)/man8/quota.8" "$(DSTROOT)/$(SHAREDIR)/man8/cyrus-quota.8")
	$(SILENT) install -m 0555 "$(SRCROOT)/$(SETUPEXTRASDIR)/cyrus" "$(DSTROOT)$(SASCRIPTSDIR)"
	$(SILENT) install -m 0640 "$(SRCROOT)/$(SETUPEXTRASDIR)/etc/cyrus.conf.default" "$(DSTROOT)$(ETCDIR)"
	$(SILENT) install -m 0640 "$(SRCROOT)/$(SETUPEXTRASDIR)/etc/imapd.conf.default" "$(DSTROOT)$(ETCDIR)"
	$(SILENT) install -m 0444 "$(SRCROOT)/$(SETUPEXTRASDIR)/CyrusIMAP.plist" "$(DSTROOT)/$(OSV_DIR)"
	$(SILENT) install -m 0444 "$(SRCROOT)/$(SETUPEXTRASDIR)/CyrusIMAP.txt" "$(DSTROOT)/$(OSL_DIR)"
	for file in $(CYRUS_TOOLS); \
	do \
		$(SILENT) install -m 755 "$(SRCROOT)/$(PROJECT_NAME)/tools/$$file" "$(DSTROOT)$(TOOLSDIR)" ; \
	done

	$(SILENT) $(ECHO) "---- Installing $(PROJECT_NAME) complete."

strip_imap_binaries:
	$(SILENT) ($(RM) -rf $(DSTROOT)/usr/bin/cyrus/include/cyrus/strhash.o)
	$(SILENT) ($(RM) -rf $(DSTROOT)/usr/bin/cyrus/lib)
	for dir in $(LOCAL_DIRS); \
	do \
		$(SILENT) if [ -d "$(DSTROOT)/usr/local/$$dir" ]; then \
			$(SILENT) ($(RM) -rf "$(DSTROOT)/usr/local/$$dir") \
		fi \
	done

.PHONY: clean installhdrs installsrc build install 

$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETCDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(TOOLSDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(TESTDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ADMINDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(PERL_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SHAREDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SASCRIPTSDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(OSV_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(OSL_DIR) :
	$(SILENT) $(MKDIRS) $@
