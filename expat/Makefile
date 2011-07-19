Project               = expat
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --mandir=\$${prefix}/share/man --disable-static
GnuNoBuild            = YES
GnuAfterInstall       = install-plist post-install

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Hack!
build::
	$(_v) $(MAKE) -C $(BuildDirectory)

post-install:
	$(CP) $(DSTROOT)/usr/bin/xmlwf $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/bin/xmlwf
	$(CP) $(DSTROOT)/usr/lib/libexpat.1.5.2.dylib $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/lib/libexpat.1.5.2.dylib

# Automatic Extract & Patch
AEP_Project    = expat
AEP_Version    = 2.0.1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff PR4333375.diff static.diff PR6295922.diff PR7160809.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 -F0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
