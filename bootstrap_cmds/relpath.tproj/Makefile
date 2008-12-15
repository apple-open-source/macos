Project = relpath
ProductType = tool
Install_Dir = /usr/local/bin

CFILES = relpath.c

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip
Extra_I_Flags = -u -s

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
