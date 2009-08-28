Project = fdisk
ProductType = tool
Install_Dir = /usr/sbin

CFILES = cmd.c disk.c fdisk.c getrawpartition.c mbr.c misc.c opendev.c part.c user.c auto.c
HFILES = cmd.h disk.h mbr.h misc.h part.h user.h util.h auto.h
MANPAGES = fdisk.8

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_CC_Flags += -I"$(SDKROOT)"/System/Library/Frameworks/System.framework/PrivateHeaders
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
