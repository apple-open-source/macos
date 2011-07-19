Project = hfs.util
ProductType = tool
Install_Dir = /System/Library/Filesystems/hfs.fs

MAN_DIR= $(DSTROOT)/usr/share/man/man8
MANPAGES= hfs.util.8

CFILES = hfsutil_main.c hfsutil_jnl.c
OTHERSRCS = \
            hfs_CD.fs.tiff hfs_CD.openfs.tiff hfs_FD.fs.tiff \
            hfs_FD.openfs.tiff hfs_HD.fs.tiff hfs_HD.openfs.tiff \
            hfs_RHD.fs.tiff hfs_RHD.openfs.tiff Info.plist \
            InfoPlist.strings PkgInfo version.plist hfs.util.8

Extra_LD_Flags = -framework CoreFoundation

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/$(Install_Dir)/Contents/Resources/English.lproj"
	install -o root -m 644 Info.plist "$(DSTROOT)/$(Install_Dir)/Contents"
	install -o root -m 644 PkgInfo "$(DSTROOT)/$(Install_Dir)/Contents"
	install -o root -m 644 version.plist "$(DSTROOT)/$(Install_Dir)/Contents"
	install -o root -m 644 InfoPlist.strings "$(DSTROOT)/$(Install_Dir)/Contents/Resources/English.lproj"
