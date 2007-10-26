Project               = subversion
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = perl-bindings python-bindings ruby-bindings \
                        post-install install-plist
Extra_Configure_Flags = --enable-shared --disable-static \
                        --without-berkeley-db \
                        --with-apxs --disable-mod-activation \
                        --with-apr=/usr --with-apr-util=/usr \
                        --with-neon=/usr/local \
                        --with-swig=/usr

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target  = install
Install_Flags   = DESTDIR=$(DSTROOT)

# Copied from perl/Extras/Makefile
installarchlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installarchlib}'))
installbin := $(subst $(DSTROOT),,$(EXTRAS)/bin)
installprivlib := $(subst Perl,Perl/Extras,$(shell perl -MConfig -e 'print $$Config::Config{installprivlib}'))
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
MAKEARGS := PASTHRU_INC='$(RC_CFLAGS)' OTHERLDFLAGS='$(RC_CFLAGS)' DESTDIR=$(DSTROOT)

perl-bindings:
	cd $(BuildDirectory) && make swig-pl PLARGS="$(PLARGS)" MAKEARGS="$(MAKEARGS)"
	cd $(BuildDirectory) && make install-swig-pl PLARGS="$(PLARGS)" MAKEARGS="$(MAKEARGS)" DESTDIR=$(DSTROOT)
	@for module in Client Core Delta Fs Ra Repos Wc; do\
		$(CP) $(DSTROOT)$(installarchlib)/auto/SVN/_$${module}/_$${module}.bundle $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(installarchlib)/auto/SVN/_$${module}/_$${module}.bundle; \
		$(RM) $(DSTROOT)$(installarchlib)/auto/SVN/_$${module}/_$${module}.bs; \
	done

# Python path setup
PYTHONLIBDIR = $(shell python -c 'import sys; print sys.prefix')/Extras/lib/python
PYDIR = $(PYTHONLIBDIR)/libsvn
PYDIR_EXTRA = $(PYTHONLIBDIR)/svn

python-bindings:
	cd $(BuildDirectory) && make swig-py
	cd $(BuildDirectory) && make install-swig-py swig_pydir=$(PYDIR) swig_pydir_extra=$(PYDIR_EXTRA) DESTDIR=$(DSTROOT)
	@for module in client core delta fs ra repos wc; do \
		$(CP) $(DSTROOT)$(PYDIR)/_$${module}.so $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(PYDIR)/_$${module}.so; \
	done

# Ruby stuff
RBDIR = $(shell ruby -r mkmf -e 'puts Config.expand($$sitearchdir)')

ruby-bindings:
	cd $(BuildDirectory) && make swig-rb
	cd $(BuildDirectory) && make install-swig-rb DESTDIR=$(DSTROOT)
	@for module in client core delta fs ra repos wc; do \
		$(CP) $(DSTROOT)$(RBDIR)/svn/ext/$${module}.bundle $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(RBDIR)/svn/ext/$${module}.bundle; \
	done

LIBEXECDIR = $(shell apxs -q LIBEXECDIR)

# Post-install cleanup
post-install:
	@for binary in svn svnadmin svndumpfilter svnlook svnserve svnsync svnversion; do \
		file=$(DSTROOT)/usr/bin/$${binary}; \
		echo $(CP) $${file} $(SYMROOT); \
		$(CP) $${file} $(SYMROOT); \
		echo $(STRIP) -x $${file}; \
		$(STRIP) -x $${file}; \
		for arch in ppc64 x86_64; do \
			echo lipo -remove $$arch -output $$file $$file; \
			lipo -remove $$arch -output $$file $$file || true; \
		done \
	done
	@for library in client delta diff fs fs_fs ra ra_dav ra_local ra_svn repos subr wc swig_perl swig_py swig_ruby; do \
		$(CP) $(DSTROOT)/usr/lib/libsvn_$${library}-1.0.0.0.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)/usr/lib/libsvn_$${library}-1.0.0.0.dylib; \
		$(RM) $(DSTROOT)/usr/lib/libsvn_$${library}-1.la; \
	done
	@for module in authz dav; do \
		$(CP) $(DSTROOT)$(LIBEXECDIR)/mod_$${module}_svn.so $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(LIBEXECDIR)/mod_$${module}_svn.so; \
	done

# Automatic Extract & Patch
AEP_Project    = subversion
AEP_Version    = 1.4.4
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff \
                 Makefile.in.diff \
                 neon.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
