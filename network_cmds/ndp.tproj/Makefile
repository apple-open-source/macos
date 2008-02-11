Project = ndp
Install_Dir = /usr/sbin

CFILES = ndp.c
HFILES = gnuc.h
MANPAGES = ndp.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DIPSEC_DEBUG -DKAME_SCOPEID
Extra_LD_Flags = -lipsec

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

