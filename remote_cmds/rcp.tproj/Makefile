Project = rcp
Install_Dir = /bin

HFILES = extern.h pathnames.h
CFILES = rcp.c util.c
MANPAGES = rcp.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

