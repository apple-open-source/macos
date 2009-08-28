Project = rsh
Install_Dir = /usr/bin
Extra_CC_Flags = -D_PATH_RLOGIN=\"/usr/bin/rlogin\"

CFILES = rsh.c
MANPAGES = rsh.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
