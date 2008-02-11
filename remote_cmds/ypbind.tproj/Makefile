Project = ypbind
Install_Dir = /usr/sbin

CFILES = ypbind.c
RPCFILES = yp.x
MANPAGES = ypbind.8
LAUNCHD_PLISTS = com.apple.nis.ypbind.plist

Extra_LD_Flags = -Wl,-bind_at_load

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
