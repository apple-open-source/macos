Project = rpc.yppasswdd
Install_Dir = /usr/libexec

HFILES = yppasswd.h
CFILES = passwd.c rpc.yppasswdd.c yppasswdd_mkpw.c yppasswdd_proc.c
MANPAGES = rpc.yppasswdd.8
LAUNCHD_PLISTS = com.apple.nis.rpc.yppasswdd.plist

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
