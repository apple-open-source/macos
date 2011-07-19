Project = umount
ProductType = tool
Install_Dir = /sbin

CFILES = umount.c
MANPAGES = umount.8

Extra_CC_Flags = -Wall -D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

INSTALL_PERMISSIONS = 555  # If set, 'install' chmod's executable to this
