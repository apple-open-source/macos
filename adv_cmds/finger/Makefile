Project = finger
Install_Dir = /usr/bin

CFILES = finger.c lprint.c net.c sprint.c util.c
MANPAGES = finger.1 finger.conf.5

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
