Project = fsck_hfs
ProductType = tool
Install_Dir = /sbin

SubProjects = dfalib

HFILES = fsck_hfs.h cache.h fsck_debug.h
CFILES = fsck_hfs.c strings.c utilities.c cache.c fsck_debug.c
MANPAGES = fsck_hfs.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1 \
	-Idfalib
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldfalib

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

HFS_INSTALLDIR = /System/Library/Filesystems/hfs.fs
FSCK_STRINGPATH = $(HFS_INSTALLDIR)/Contents/Resources/English.lproj/fsck.strings

after_install:
	$(RMDIR) "$(DSTROOT)/scratch"
	$(INSTALL_DIRECTORY) "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents/Resources/English.lproj
	printf "\376\377" > "$(DSTROOT)$(FSCK_STRINGPATH)"
	awk -f makestrings strings.c | tr \\012 \\015 | sed 's/./~&/g' | tr -d \\012 | tr \\015 \\012 | tr \\176 \\000 >> "$(DSTROOT)$(FSCK_STRINGPATH)"
	$(CHOWN) -Rf root:wheel "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents
	find "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents -type d -exec chmod 755 {} \;
	find "$(DSTROOT)$(HFS_INSTALLDIR)"/Contents -type f -exec chmod 644 {} \;
