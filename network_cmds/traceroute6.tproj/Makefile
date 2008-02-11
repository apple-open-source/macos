Project = traceroute6
Install_Dir = /usr/sbin

CFILES = traceroute6.c
MANPAGES = traceroute6.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DIPSEC
Extra_LD_Flags = -lipsec

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
