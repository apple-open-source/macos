Project = traceroute
Install_Dir = /usr/sbin

CFILES = traceroute.c ifaddrlist.c findsaddr-socket.c version.c
MANPAGES = traceroute.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-DHAVE_SOCKADDR_SA_LEN

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555
