Project = vsdbutil
ProductType = tool
Install_Dir = /usr/sbin

CFILES = vsdbutil_main.c
MANPAGES = vsdbutil.8

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_DIRECTORY) "$(DSTROOT)/System/Library/LaunchDaemons"
	$(INSTALL_FILE) -m 644 -o root -g wheel com.apple.vsdbutil.plist "$(DSTROOT)/System/Library/LaunchDaemons/com.apple.vsdbutil.plist"
