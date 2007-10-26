Project               = apr
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = install-plist post-install munge-build-files
Extra_Configure_Flags = --with-installbuilddir=\$${prefix}/share/apr-1/build-1 \
                        --enable-dso=dlfcn
Extra_Environment     = apr_lock_method="USE_FLOCK_SERIALIZE"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

post-install:
	$(CP) $(DSTROOT)/usr/lib/libapr-1.0.2.7.dylib $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/lib/libapr-1.0.2.7.dylib
	$(RM) $(DSTROOT)/usr/lib/apr.exp
	$(RM) $(DSTROOT)/usr/lib/libapr-1.la
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libapr-1.a $(DSTROOT)/usr/local/lib

munge-build-files:
	ed - $(DSTROOT)/usr/share/apr-1/build-1/apr_rules.mk < $(SRCROOT)/files/fix-apr_rules.mk.ed
	ed - $(DSTROOT)/usr/share/apr-1/build-1/libtool < $(SRCROOT)/files/fix-libtool.ed

# Automatic Extract & Patch
AEP_Project    = apr
AEP_Version    = 1.2.7
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff LP64.diff sendfile.diff unicode.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt
