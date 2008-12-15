Project = fuser
ProductType = tool
Install_Dir = /usr/bin

MANPAGES = fuser.1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) fuser.pl "$(DSTROOT)$(Install_Dir)"/fuser
