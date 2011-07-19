Project        = mod_perl
ProjectVersion = $(Project)-2.0.5

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# multi-version support
VERSIONERDIR := /usr/local/versioner

# Perl multi-version support
PERLVERSIONS := $(VERSIONERDIR)/perl/versions
PERLSUBDEFAULT := $(shell sed -n '/^DEFAULT = /s///p' $(PERLVERSIONS))
PERLDEFAULT := $(shell grep '^$(PERLSUBDEFAULT)' $(PERLVERSIONS))
PERLUNORDEREDVERS := $(shell grep -v '^DEFAULT' $(PERLVERSIONS))
# do default version last
PERLORDEREDVERS := $(filter-out $(PERLDEFAULT),$(PERLUNORDEREDVERS)) $(PERLDEFAULT)

PERLEXTRASLIB := $(subst Perl,Perl/Extras,$(shell perl -e 'require Config; print $$Config::Config{installprivlib}'))
PERLARCHLIB := $(shell perl -e 'require Config; print $$Config::Config{installarchlib}')
PERLEXTRASARCHLIB := $(subst Perl,Perl/Extras,$(PERLARCHLIB))

install::
	@echo "--> Extracting..."
	$(TAR) -C $(OBJROOT) -zxf $(SRCROOT)/$(ProjectVersion).tar.gz

	@echo "--> Building/installing..."
	@set -x && \
	cd $(OBJROOT)/$(ProjectVersion) && \
	for vers in $(PERLORDEREDVERS); do \
		export VERSIONER_PERL_VERSION=$${vers} && \
	    installarchlib=`perl -MConfig -e 'print $$Config::Config{installarchlib}' | sed 's,Perl,Perl/Extras,'` && \
	    installprivlib=`perl -MConfig -e 'print $$Config::Config{installprivlib}' | sed 's,Perl,Perl/Extras,'` && \
		ARCHFLAGS="$(RC_CFLAGS)" perl Makefile.PL \
		MP_APXS="/usr/sbin/apxs" \
		MP_CCOPTS="$(CFLAGS)" \
		INSTALLARCHLIB=$${installarchlib} \
		INSTALLDIRS=perl \
		INSTALLMAN3DIR="$(MANDIR)/man3" && \
		$(MAKE) && \
		$(MAKE) install DESTDIR=$(DSTROOT); \
	done

	@echo "--> Post install cleanup..."
	find $(DSTROOT) -name \*.bs -delete
	find $(DSTROOT) -name perllocal.pod -delete
	find $(DSTROOT) -type d -empty -delete

	@set -x && \
	cd $(DSTROOT) && \
	for bundle in `find . -type f -name \*.bundle -o -name \*.so`; do \
		bundledir=$(SYMROOT)/`dirname $${bundle}` && \
		$(MKDIR) $${bundledir} && \
		$(CP) $${bundle} $${bundledir} && \
		$(STRIP) -x $${bundle}; \
	done

	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/OpenSourceVersions
	$(INSTALL_FILE) $(SRCROOT)/mod_perl.plist $(DSTROOT)/usr/local/OpenSourceVersions/apache_mod_perl.plist
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/OpenSourceLicenses
	$(INSTALL_FILE) $(OBJROOT)/$(ProjectVersion)/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/apache_mod_perl.txt
	$(MKDIR) $(DSTROOT)/usr/share/man/man1
	$(INSTALL_FILE) $(SRCROOT)/mp2bug.1 $(DSTROOT)/usr/share/man/man1
	$(_v) $(MAKE) compress_man_pages
