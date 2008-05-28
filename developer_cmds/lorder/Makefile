Project = lorder
Install_Dir = /usr/bin

MANPAGES = lorder.1

Extra_CC_Flags = -mdynamic-no-pic -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) lorder.sh $(DSTROOT)/$(Install_Dir)/lorder
