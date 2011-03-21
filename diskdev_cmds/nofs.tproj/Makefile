Project = nofs.util
ProductType = tool
Install_Dir = /System/Library/Filesystems/nofs.fs/Contents/Resources

CFILES = nofsutil.c ui.c

Extra_CC_Flags = -Wall -mdynamic-no-pic -Wuninitialized -Wunused
Extra_LD_Flags = -dead_strip
Extra_Frameworks = -framework CoreFoundation -framework DiskArbitration -framework IOKit -framework SystemConfiguration

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

RC_ARCHS = i386 x86_64

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)$(Install_Dir)"/English.lproj
	$(INSTALL_FILE) Info.plist "$(DSTROOT)$(Install_Dir)"/..
	$(INSTALL_FILE) PkgInfo "$(DSTROOT)$(Install_Dir)"/..
	$(INSTALL_FILE) InfoPlist.strings "$(DSTROOT)$(Install_Dir)"/English.lproj
	$(INSTALL_FILE) Localizable.strings "$(DSTROOT)$(Install_Dir)"/English.lproj
	#find $(DSTROOT)$(INSTALLDIR)/.. -type d -exec chmod 755 {} \;
	#find $(DSTROOT)$(INSTALLDIR)/.. -type f -exec chmod 644 {} \;
