Project = colldef
Install_Dir = /usr/bin

YFILES = parse.y
LFILES = scan.l
MANPAGES = colldef.1

Extra_L_Flags = -8 -i

Extra_CC_Flags = -mdynamic-no-pic
Extra_CC_Flags += -I. -I./locale
Extra_CC_Flags += -include fbsdcompat.h
Extra_CC_Flags += -DCOLLATE_DEBUG -DYY_NO_UNPUT
Extra_CC_Flags += -Wall

Extra_LD_Flags = -dead_strip
Extra_LD_Flags += -ll

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
