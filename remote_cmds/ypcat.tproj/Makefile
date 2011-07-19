Project = ypcat
Install_Dir = /usr/bin

CFILES = ypcat.c
MANPAGES = ypcat.1

Extra_CC_Flags = -Wall -Werror -fPIE -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
