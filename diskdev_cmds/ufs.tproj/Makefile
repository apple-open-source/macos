Project = ufs.util
ProductType = tool
Install_Dir = /System/Library/Filesystems/ufs.fs

CFILES = ufsutil.c
MANPAGES = ufs.util.8

#OTHERSRCS = Makefile.preamble Makefile Makefile.postamble Info.plist\
            InfoPlist.strings PkgInfo version.plist ufs.util.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-I../disklib
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk 

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)$(Install_Dir)"/Contents/Resources/English.lproj
	$(INSTALL_FILE) Info.plist "$(DSTROOT)$(Install_Dir)"/Contents
	$(INSTALL_FILE) PkgInfo "$(DSTROOT)$(Install_Dir)"/Contents
	$(INSTALL_FILE) version.plist "$(DSTROOT)$(Install_Dir)"/Contents
	$(INSTALL_FILE) InfoPlist.strings "$(DSTROOT)$(Install_Dir)"/Contents/Resources/English.lproj
	#find $(DSTROOT)$(INSTALLDIR)/Contents -type d -exec chmod 755 {} \;
	#find $(DSTROOT)$(INSTALLDIR)/Contents -type f -exec chmod 644 {} \;
