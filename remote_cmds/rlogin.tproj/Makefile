Project = rlogin
Install_Dir = /usr/bin

CFILES = rlogin.c
MANPAGES = rlogin.1

Extra_CC_Flags = -Wall -Werror -fPIE
Extra_LD_Flags = -dead_strip -pie

Extra_CC_Flags += -D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 0555

after_install:
	/usr/bin/codesign --force --sign - --entitlements entitlements.plist $(DSTROOT)$(Install_Dir)/$(Project)
