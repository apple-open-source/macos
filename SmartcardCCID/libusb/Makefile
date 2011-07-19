Project               = libusb
UserType              = Administrator
ToolType              = Libraries
GnuAfterInstall       = remove-autom4te-cache remove-shared-libusb
Extra_Configure_Flags = 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Automatic Extract & Patch
AEP_Project    = libusb
AEP_Version    = 0.1.13b
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = FixConfigureAddCPP.patch skipConfigure.patch remove_config_libdir.patch darwinwarnings.patch allowWarnings.patch
#svn co -r658 https://libusb.svn.sourceforge.net/svnroot/libusb/trunk

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -xzf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done

	(cd $(SRCROOT)/$(Project) && ./autogen.sh --skip-configure )
	
remove-autom4te-cache:
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache

remove-shared-libusb:
	rm -f $(DSTROOT)/usr/lib/libusb*{dylib,la}
