Project        = mod_perl
ProjectVersion = $(Project)-2.0.2
Patches        = patch-lib__Apache2__Build.pm patch-lib__ModPerl__BuildMM.pm CVE-2007-1349.diff

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

PERLEXTRASLIB := $(subst Perl,Perl/Extras,$(shell perl -e 'require Config; print $$Config::Config{installprivlib}'))
PERLARCHLIB := $(shell perl -e 'require Config; print $$Config::Config{installarchlib}')
PERLEXTRASARCHLIB := $(subst Perl,Perl/Extras,$(PERLARCHLIB))

install::
	$(TAR) -C $(OBJROOT) -zxf $(SRCROOT)/$(ProjectVersion).tar.gz
	@for patch in $(Patches); do \
		cd $(OBJROOT)/$(ProjectVersion) && patch -p0 < $(SRCROOT)/files/$${patch}; \
	done
	cd $(OBJROOT)/$(ProjectVersion) && perl Makefile.PL \
		MP_APXS=/usr/sbin/apxs \
		MP_CCOPTS="$(CFLAGS)" \
		INSTALLSITELIB=$(PERLEXTRASLIB) \
		INSTALLSITEARCH=$(PERLEXTRASARCHLIB) \
		INSTALLSITEMAN3DIR=$(MANDIR)/man3
	$(MAKE) -C $(OBJROOT)/$(ProjectVersion)
	$(MAKE) -C $(OBJROOT)/$(ProjectVersion) install DESTDIR=$(DSTROOT)
	$(RM) $(DSTROOT)/$(PERLARCHLIB)/perllocal.pod
	$(STRIP) -x $(DSTROOT)$(shell apxs -q LIBEXECDIR)/mod_perl.so
	$(STRIP) -x $(DSTROOT)$(PERLEXTRASARCHLIB)/auto/*/*.bundle
	$(STRIP) -x $(DSTROOT)$(PERLEXTRASARCHLIB)/auto/*/*/*.bundle
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/OpenSourceVersions
	$(INSTALL_FILE) $(SRCROOT)/mod_perl.plist $(DSTROOT)/usr/local/OpenSourceVersions/apache_mod_perl.plist
	$(INSTALL_DIRECTORY) $(DSTROOT)/usr/local/OpenSourceLicenses
	$(INSTALL_FILE) $(OBJROOT)/$(ProjectVersion)/LICENSE $(DSTROOT)/usr/local/OpenSourceLicenses/apache_mod_perl.txt
