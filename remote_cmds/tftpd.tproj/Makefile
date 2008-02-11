Project = tftpd
Install_Dir = /usr/libexec

HFILES = tftp.h tftpsubs.h
CFILES = tftpd.c tftpsubs.c
MANPAGES = tftpd.8
LAUNCHD_PLISTS = tftp.plist tftp.plist-SERVER

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(MKDIR) "$(DSTROOT)/private/tftpboot"
