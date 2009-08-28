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
SPAM_TAR_GZ=Mail-SpamAssassin-3.2.1.tar.gz

SPAM_BUILD_DIR=/spamassassin/Mail-SpamAssassin-3.2.1

PRIV_DIR=/private
USR_DIR=/usr
PERL_VER=`perl -V:version | sed -n -e "s/[^0-9.]*\([0-9.]*\).*/\1/p"`
PERL_DIR=/System/Library/Perl
PERL_EXTRA_DIR=/System/Library/Perl/Extras/Extras
PERL_EXTRA_VER_DIR=/System/Library/Perl/Extras/$(PERL_VER)

SETUPEXTRASDIR=SetupExtras
SASCRIPTSDIR=/System/Library/ServerSetup/SetupExtras
SA_BIN_DIR=/private/etc/mail/spamassassin
USR=/usr
USR_LIB=/usr/lib
USR_LOCAL=/usr/local
USR_OS_VERSION=$(USR_LOCAL)/OpenSourceVersions
USR_OS_LICENSE=$(USR_LOCAL)/OpenSourceLicenses

STRIP=/usr/bin/strip
GNUTAR=/usr/bin/gnutar
CHOWN=/usr/sbin/chown

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

default:: make_sa

install :: make_sa_install

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

make_sa_install :
	$(SILENT) $(ECHO) "--------- Make Install Spam Assassin ---------"

	# Create install directories
	install -d -m 0755 "$(DSTROOT)$(PRIV_DIR)"
	install -d -m 0755 "$(DSTROOT)$(USR_DIR)"
	install -d -m 0755 "$(DSTROOT)$(PERL_EXTRA_VER_DIR)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_VERSION)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_LICENSE)"
	install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons

	# Build SpamAssassin
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && perl Makefile.PL PREFIX=/ DESTDIR=$(DSTROOT))
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make CFLAGS="$(CFLAGS)")
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make CFLAGS="$(CFLAGS)" install)

	# Copy files to OS X hierarchy
	$(SILENT) ($(MV) "$(DSTROOT)/bin" "$(DSTROOT)/usr/bin")
	$(SILENT) ($(MV) "$(DSTROOT)/etc" $(DSTROOT)$(PRIV_DIR))
	$(SILENT) ($(MV) "$(DSTROOT)/share" $(DSTROOT)$(USR_DIR))
	$(SILENT) ($(MV) "$(DSTROOT)/lib/perl5/site_perl/Mail" $(DSTROOT)$(PERL_EXTRA_VER_DIR)/)
	$(SILENT) ($(MV) "$(DSTROOT)/lib/perl5/site_perl/spamassassin-run.pod" $(DSTROOT)$(PERL_EXTRA_VER_DIR)/Mail/)
	$(SILENT) ($(RM) -r "$(DSTROOT)/lib")
	install -m 0644 "$(SRCROOT)/SpamAssassin.SetupExtras/local.cf" "$(DSTROOT)$(SA_BIN_DIR)/local.cf"
	install -m 0755 "$(SRCROOT)/SpamAssassin.SetupExtras/learn_junk_mail" "$(DSTROOT)$(SA_BIN_DIR)/learn_junk_mail"

	# Clean up of files & build directories
	$(SILENT) ($(CD) "$(SRCROOT)/SpamAssassin" && make clean)
	$(SILENT) ($(CD) "$(SRCROOT)" && $(RM) ./SpamAssassin/Makefile.old)
	$(SILENT) ($(RM) "$(DSTROOT)/System/Library/Perl/5.10.0/darwin-thread-multi-2level/perllocal.pod" )

	# Install Open Souce files
	install -m 0444 $(SRCROOT)/SpamAssassin.OpenSourceInfo/SpamAssassin.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/SpamAssassin.OpenSourceInfo/SpamAssassin.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 0644 $(SRCROOT)/SpamAssassin.LaunchDaemons/com.apple.learnjunkmail.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.learnjunkmail.plist
	install -m 0644 $(SRCROOT)/SpamAssassin.LaunchDaemons/com.apple.updatesa.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.updatesa.plist
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/MigrationExtras
	install -m 0755 $(SRCROOT)/SpamAssassin.SetupExtras/upgrade_learn_sa \
			$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/66_spam_assassin_migrator

	$(SILENT) $(ECHO) "--------- Building Spam Assassin complete ---------"

.PHONY: clean installhdrs installsrc build install 
