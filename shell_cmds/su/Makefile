Project = su

CFILES = su.c
MANPAGES = su.1

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip \
	-lpam -lpam_misc -lbsm

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
