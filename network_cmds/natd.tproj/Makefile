Project = natd
Install_Dir = /usr/sbin

HFILES = natd.h
CFILES = icmp.c natd.c
MANPAGES = natd.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I../alias
Extra_LD_Flags = -L${DSTROOT}/usr/lib -lalias

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
