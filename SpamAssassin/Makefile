#
# xbs-compatible wrapper Makefile for SpamAssassin
#

PROJECT=SpamAssassin

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

PROJECT_NAME=SpamAssassin
CLAMAV_NAME=clamav-0.88.2.tar.gz

AMAVIS_DIR=/private/var/amavis
CLAMAV_BUILD_DIR=/clamav/clamav-0.88.2
VIRUS_MAILS_DIR=/private/var/virusmails
ETCDIR=/private/etc
ETC_SPAMA_DIR=/private/etc/mail/spamassassin
ETC_CLAMAV_DIR=/private/etc/spam/clamav
SHAREDIR=/usr/share/man
SETUPEXTRASDIR=SetupExtras
SASCRIPTSDIR=/System/Library/ServerSetup/SetupExtras
LAUNCHDDIR=/System/Library/LaunchDaemons
SA_BIN_DIR=/private/etc/mail/spamassassin
PERL_VER=`perl -V:version | sed -n -e "s/[^0-9.]*\([0-9.]*\).*/\1/p"`
PERL_DIR=/System/Library/Perl
PERL_EXTRA_DIR=/System/Library/Perl/Extras
PERL_EXTRA_VER_DIR=$(PERL_EXTRA_DIR)/$(PERL_VER)

STRIP=/usr/bin/strip
GNUTAR=/usr/bin/gnutar
CHOWN=/usr/sbin/chown

# Clam Antivirus config
#

CLAMAV_CONFIG= \
	--prefix=/ \
	--exec-prefix=/usr \
	--bindir=/usr/bin \
	--sbindir=/usr/sbin \
	--libexecdir=/usr/libexec \
	--datadir=/usr/share/clamav \
	--sysconfdir=/private/etc/spam/clamav \
	--sharedstatedir=/user/share/clamav/com \
	--localstatedir=/private/var/clamav \
	--libdir=/usr/lib \
	--includedir=/usr/share/clamav/include \
	--oldincludedir=/usr/share/clamav/include \
	--infodir=/usr/share/clamav/info \
	--mandir=/usr/share/man \
	--with-dbdir=/private/var/clamav \
	--disable-shared \
        --with-user=0 \
        --with-group=0 \
	--enable-static

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: make_clamav

install :: make_sa_install make_clamav_install make_amavisd_install

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

make_sa :
	$(SILENT) $(ECHO) "----------- Make Spam Assassin -----------"
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && perl Makefile.PL PREFIX=/ DESTDIR=$(DSTROOT))
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make CFLAGS="$(CFLAGS))

make_clamav :
	$(SILENT) $(ECHO) "------------ Make Clam AV ------------"
	$(SILENT) if [ -e "$(SRCROOT)/clamav/$(CLAMAV_NAME)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/clamav" && $(GNUTAR) -xzpf "$(CLAMAV_NAME)") ; \
	fi
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && ./configure $(CLAMAV_CONFIG))
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && make CFLAGS="$(CFLAGS)")

make_clamav_install :  $(DSTROOT)$(ETCDIR) $(DSTROOT)$(ETC_CLAMAV_DIR) $(DSTROOT)$(SASCRIPTSDIR)
	$(SILENT) $(ECHO) "------------ Make Install Clam AV ------------"
	$(SILENT) if [ -e "$(SRCROOT)/clamav/$(CLAMAV_NAME)" ]; then\
		$(SILENT) ($(CD) "$(SRCROOT)/clamav" && $(GNUTAR) -xzpf "$(CLAMAV_NAME)") ; \
	fi
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && ./configure $(CLAMAV_CONFIG))
	$(SILENT) ($(CD) "$(SRCROOT)$(CLAMAV_BUILD_DIR)" && make "DESTDIR=$(SRCROOT)/Extra/dest" CFLAGS="$(RC_CFLAGS)" install)
	$(SILENT) ($(CD) "$(SRCROOT)/Extra/dest" && $(CP) -rpf * "$(DSTROOT)")
	$(SILENT) ($(CD) "$(SRCROOT)/Extra/etc" && $(CP) -rpf *.conf "$(DSTROOT)$(ETC_CLAMAV_DIR)/")
	$(SILENT) ($(CD) "$(DSTROOT)" && $(CHOWN) -R root:wheel *)
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(ETC_CLAMAV_DIR)")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)/usr/bin/clamdscan")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)/usr/bin/clamscan")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)/usr/bin/freshclam")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)/usr/bin/sigtool")
	$(SILENT) ($(RM) -rf "$(DSTROOT)/usr/lib/libclamav.a")
	$(SILENT) ($(RM) -rf "$(DSTROOT)/usr/lib/libclamav.la")
	$(SILENT) ($(STRIP) -S "$(DSTROOT)/usr/sbin/clamd")
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)/private/var/clamav")
	$(SILENT) (/bin/chmod 755 "$(DSTROOT)/private/var/clamav")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)/private/var/clamav/daily.cvd")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)/private/var/clamav/main.cvd")
	$(SILENT) ($(CP) "$(SRCROOT)/$(SETUPEXTRASDIR)/clamav" "$(DSTROOT)$(SASCRIPTSDIR)")
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/etc/clamav.conf" "$(DSTROOT)/private/etc/")
	$(SILENT) ($(CHOWN) root "$(DSTROOT)/private/etc/clamav.conf")
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/etc/freshclam.conf" "$(DSTROOT)/private/etc/")
	$(SILENT) ($(CHOWN) root "$(DSTROOT)/private/etc/freshclam.conf")
	$(SILENT) ($(CHOWN) -R root:wheel "$(DSTROOT)/private/etc/spam")
	$(SILENT) $(ECHO) "---- Building Clam AV complete."

make_amavisd_install : $(DSTROOT)$(AMAVIS_DIR) $(DSTROOT)$(VIRUS_MAILS_DIR) $(DSTROOT)$(LAUNCHDDIR)
	$(SILENT) $(ECHO) "-------------- Amavisd-new --------------"
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/System/Library/LaunchDaemons/org.amavis.amavisd.plist" "$(DSTROOT)$(LAUNCHDDIR)/")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)$(LAUNCHDDIR)/org.amavis.amavisd.plist")
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/etc/amavisd.conf" "$(DSTROOT)/private/etc/")
	$(SILENT) ($(CHOWN) root "$(DSTROOT)/private/etc/amavisd.conf")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)/private/etc/amavisd.conf")
	$(SILENT) ($(CP) "$(SRCROOT)/amavisd/amavisd" "$(DSTROOT)/usr/bin/")
	$(SILENT) ($(CHOWN) root "$(DSTROOT)/usr/bin/amavisd")
	$(SILENT) (/bin/chmod 755 "$(DSTROOT)/usr/bin/amavisd")
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(AMAVIS_DIR)")
	$(SILENT) (/bin/chmod 750 "$(DSTROOT)$(AMAVIS_DIR)")
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(VIRUS_MAILS_DIR)")
	$(SILENT) (/bin/chmod 750 "$(DSTROOT)$(VIRUS_MAILS_DIR)")
	$(SILENT) (/bin/echo "\n" > "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")
	$(SILENT) ($(CHOWN) -R clamav:clamav "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")
	$(SILENT) (/bin/chmod 644 "$(DSTROOT)$(AMAVIS_DIR)/whitelist_sender")
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/usr/share/man/man8/"* "$(DSTROOT)/usr/share/man/man8/")
	$(SILENT) $(ECHO) "---- Building Amavisd-new complete."

make_sa_install : $(DSTROOT)$(SHAREDIR) $(DSTROOT)$(ETCDIR) $(DSTROOT)$(ETC_SPAMA_DIR) $(DSTROOT)$(PERL_EXTRA_DIR) $(DSTROOT)$(SA_BIN_DIR)
	$(SILENT) $(ECHO) "--------- Make Install Spam Assassin ---------"
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && perl Makefile.PL PREFIX=/ DESTDIR=$(DSTROOT))
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make CFLAGS="$(CFLAGS)" install)
	$(SILENT) ($(CD) "$(DSTROOT)/etc" && $(CP) -rpf * $(DSTROOT)$(ETCDIR))
	$(SILENT) ($(CD) "$(DSTROOT)" && $(RM) -rf "$(DSTROOT)/etc")
	$(SILENT) ($(CD) "$(DSTROOT)/man" && $(CP) -rpf * "$(DSTROOT)$(SHAREDIR)")
	$(SILENT) ($(CD) "$(DSTROOT)" && $(RM) -rf "$(DSTROOT)/man")
	$(SILENT) if [ -d "$(DSTROOT)/lib/perl5/site_perl" ]; then\
		$(SILENT) ($(CP) -rpf "$(DSTROOT)/lib/perl5/site_perl/"* "$(DSTROOT)$(PERL_EXTRA_DIR)"); \
	fi
	$(SILENT) if [ -d "$(DSTROOT)/$(PERL_EXTRA_VER_DIR)/darwin-thread-multi-2level" ]; then\
		$(SILENT) ($(RM) -rf "$(DSTROOT)/$(PERL_EXTRA_VER_DIR)/darwin-thread-multi-2level"); \
	fi
	$(SILENT) if [ -d "$(DSTROOT)/$(PERL_EXTRA_DIR)/Mail" ]; then\
		$(SILENT) ($(CP) -rpf "$(DSTROOT)/$(PERL_EXTRA_DIR)/Mail" "$(DSTROOT)/$(PERL_EXTRA_VER_DIR)/"); \
		$(SILENT) ($(RM) -rf "$(DSTROOT)/$(PERL_EXTRA_DIR)/Mail"); \
	fi
	$(SILENT) if [ -d "$(DSTROOT)/lib" ]; then\
		$(SILENT) ($(CD) "$(DSTROOT)" && $(RM) -rf "$(DSTROOT)/lib"); \
	fi
	$(SILENT) if [ -d "$(DSTROOT)/$(PERL_DIR)/$(PERL_VER)" ]; then\
		$(SILENT) ($(RM) -rf "$(DSTROOT)/$(PERL_DIR)/$(PERL_VER)"); \
	fi
	$(SILENT) ($(STRIP) -S $(DSTROOT)/usr/bin/spamd)
	$(SILENT) ($(STRIP) -S $(DSTROOT)/usr/bin/spamc)
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make clean)
	$(SILENT) ($(CD) "$(SRCROOT)" && $(RM) ./SpamAssassin/Makefile.old)
	$(SILENT) ($(CP) "$(SRCROOT)/Extra/etc/mail/spamassassin/local.cf" "$(DSTROOT)$(ETC_SPAMA_DIR)")
	$(SILENT) (install -m 0755 "$(SRCROOT)/$(SETUPEXTRASDIR)/learn_junk_mail" "$(DSTROOT)$(SA_BIN_DIR)" )
	$(SILENT) $(ECHO) "---- Building Spam Assassin complete."

make_modules_install : $(DSTROOT)$(PERL_EXTRA_DIR)
	$(SILENT) $(ECHO) "-------------- Perl Modules ---------------"
	$(SILENT) $(ECHO) "---- Building Perl Modules complete."

.PHONY: clean installhdrs installsrc build install 

$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETCDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETC_CLAMAV_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETC_SPAMA_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SHAREDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(AMAVIS_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(VIRUS_MAILS_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SASCRIPTSDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LAUNCHDDIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(PERL_EXTRA_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(SA_BIN_DIR) :
	$(SILENT) $(MKDIRS) $@
