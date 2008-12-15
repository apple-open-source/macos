Project = mount_portal
ProductType = tool
Install_Dir = /sbin

HFILES = pathnames.h portald.h
CFILES = activate.c conf.c mount_portal.c pt_conf.c pt_exec.c\
         pt_file.c pt_tcp.c
MANPAGES = mount_portal.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D_DARWIN_USE_64_BIT_INODE=1
Extra_LD_Flags = -dead_strip \
	-L"$(SYMROOT)" -ldisk

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
