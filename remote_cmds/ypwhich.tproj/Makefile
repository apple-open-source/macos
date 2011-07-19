Project = ypwhich
Install_Dir = /usr/bin

CFILES = ypwhich.c
MANPAGES = ypwhich.1

Extra_CC_Flags = -Wall -Werror -fPIE -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
