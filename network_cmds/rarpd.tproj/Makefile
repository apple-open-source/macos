Project = rarpd
Install_Dir = /usr/sbin

CFILES = rarpd.c
MANPAGES = rarpd.8

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-I. -DTFTP_DIR=\"/tftpboot\"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
