Project = newfs_hfs
ProductType = tool
Install_Dir = /sbin

HFILES = hfs_endian.h newfs_hfs.h readme.h
CFILES = hfs_endian.c makehfs.c newfs_hfs.c
MANPAGES = newfs_hfs.8
	      #hfsbootdata.img newfs_hfs.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_LONG_LONG -Wno-four-char-constants
Extra_LD_Flags = -dead_strip -lutil
Extra_Frameworks = -framework CoreFoundation -framework DiskArbitration

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

HFSBOOTDATA_DIR = /usr/share/misc
after_install:
	$(MKDIR) "$(DSTROOT)/$(HFSBOOTDATA_DIR)"
	$(INSTALL_FILE) hfsbootdata.img "$(DSTROOT)/$(HFSBOOTDATA_DIR)"/hfsbootdata
