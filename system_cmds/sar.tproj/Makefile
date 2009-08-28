Project = sar
Install_Dir = /usr/bin

HFILES = sar.h
CFILES = sar.c
MANPAGES = sar.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -I. -I../sadc.tproj

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
