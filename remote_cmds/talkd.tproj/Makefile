Project = ntalkd
Install_Dir = /usr/libexec

HFILES = talkd.h
CFILES = announce.c print.c process.c table.c talkd.c ../wall.tproj/ttymsg.c
MANPAGES = ntalkd.8
LAUNCHD_PLISTS =  ntalk.plist

Extra_CC_Flags = -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
