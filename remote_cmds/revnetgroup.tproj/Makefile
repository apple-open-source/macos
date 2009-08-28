Project = revnetgroup
Install_Dir = /usr/sbin

HFILES = hash.h
CFILES = hash.c parse_netgroup.c revnetgroup.c
MANPAGES = revnetgroup.8

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
