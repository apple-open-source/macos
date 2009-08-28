Project = lorder
Install_Dir = /usr/bin

MANPAGES = lorder.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) lorder.sh $(DSTROOT)/$(Install_Dir)/lorder
