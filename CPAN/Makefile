# Makefile orchestrating CPAN

PROJECTS = \
    MLDBM \
    Mac-Errors \
    Time-Epoch \
    URI \
    libwww-perl \
    Mac-Carbon \
    Mac-Apps-Launch \
    Mac-AppleEvents-Simple \
    Mac-OSA-Simple \
    Mac-Glue \
    Convert-BinHex \
    Convert-UUlib \
    Compress-Zlib \
    IO-String \
    IO-Zlib \
    Archive-Tar \
    Archive-Zip \
    BerkeleyDB \
    MailTools \
    IO-stringy \
    MIME-tools \
    Convert-TNEF \
    Digest-SHA1 \
    HTML-Tagset \
    HTML-Parser \
    Net-Server \
    Unix-Syslog \
    Socket6 \
    IO-Socket-INET6 \
    Digest-HMAC \
    Net-IP \
    Net-DNS \
    Proc-Reliable \
    Carp-Clan \
    Bit-Vector \
    Date-Calc \
    Convert-ASN1 \
    GSSAPI \
    Net_SSLeay.pm \
    IO-Socket-SSL \
    Authen-SASL \
    XML-NamespaceSupport \
    XML-SAX \
    perl-ldap \
    DBI \
    DBD-SQLite \
    XML-LibXML-Common \
    XML-LibXML \
    XML-Parser \
    XML-Writer \
    XML-XPath \
    XML-Simple \
    IO-Tty \
    Expect \
    Regexp-Common \
    Pod-Readme \
    ExtUtils-CBuilder \
    ExtUtils-ParseXS \
    version \
    Module-Build \
    Module-Pluggable \
    Alien-wxWidgets \
    Wx \
    Class-Accessor \
    File-Slurp \
    Wx-Demo \
    Net-CIDR-Lite \
    Sys-Hostname-Long \
    Mail-SPF-Query

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
PERL = $(DSTROOT)/System/Library/Perl
VERSION := $(shell perl -e 'printf "%vd", $$^V')
ARCHLIB := $(shell perl -MConfig -e 'print $$Config::Config{archname}')
EXTRAS = $(PERL)/Extras
EXTRASPERL = $(EXTRAS)/$(VERSION)
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installbin := $(shell perl -MConfig -e 'print $$Config::Config{installbin}')
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
make := $(SRCROOT)/make.pl
MAKEARGS := DESTDIR=$(DSTROOT)
SLP = $(PERL)/$(VERSION)
export PERL5LIB := $(EXTRASPERL)
export NO_PERL_PREPENDTOPATH := 1

no_target:
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C $$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done

install:
	@set -x && for i in $(PROJECTS); do \
	    $(make) -C $$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done
	@echo ================ post-install fixups ================
	rm -f $(EXTRASPERL)/$(ARCHLIB)/perllocal.pod
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name \*.bundle -print -exec strip -x {} \;
	find $(EXTRASPERL)/$(ARCHLIB)/auto -name .packlist -print -delete
	/Developer/Makefiles/bin/compress-man-pages.pl "$(DSTROOT)/usr/share/man"

installhdrs:
	@echo CPAN has no headers to install

installsrc:
	ditto . $(SRCROOT)

clean:
