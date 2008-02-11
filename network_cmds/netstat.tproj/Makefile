Project = netstat
Install_Dir = /usr/sbin

Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)

HFILES = netstat.h
CFILES = data.c if.c inet.c inet6.c ipsec.c main.c mcast.c mbuf.c\
         route.c tp_astring.c unix.c
ifneq ($(Embedded),YES)
CFILES += mroute.c mroute6.c
endif

MANPAGES = netstat.1

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DINET6 -DIPSEC \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
