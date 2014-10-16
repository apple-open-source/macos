Project               = libusb
UserType              = Administrator
ToolType              = Libraries
GnuAfterInstall       = remove-autom4te-cache remove-shared-libusb
Extra_Configure_Flags = 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Automatic Extract & Patch
AEP_Project    = libusb
AEP_Version    = 1.0.9
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = darwin_no_seize.patch
#svn co -r658 https://libusb.svn.sourceforge.net/svnroot/libusb/trunk

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -xzf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$patchfile) || exit 1; \
	done
	
remove-autom4te-cache:
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache

remove-shared-libusb:
	rm -f $(DSTROOT)/usr/lib/libusb*{dylib,la}
