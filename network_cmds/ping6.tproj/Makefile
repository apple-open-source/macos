Project = ping6
Install_Dir = /sbin

CFILES = ping6.c md5.c
HFILES = md5.h
MANPAGES = ping6.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DIPSEC_DEBUG -DKAME_SCOPEID -DIPSEC
Extra_LD_Flags = -lipsec

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
