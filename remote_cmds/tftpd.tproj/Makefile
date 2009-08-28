Project = tftpd
Install_Dir = /usr/libexec

HFILES = tftpsubs.h
CFILES = tftpd.c ../tftp.tproj/tftpsubs.c
MANPAGES = tftpd.8
LAUNCHD_PLISTS = tftp.plist tftp.plist-SERVER

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_CC_Flags += -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(MKDIR) "$(DSTROOT)/private/tftpboot"
