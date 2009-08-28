Project = rwhod
Install_Dir = /usr/sbin

CFILES = rwhod.c
MANPAGES = rwhod.8

Extra_CC_Flags = -Wall -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
