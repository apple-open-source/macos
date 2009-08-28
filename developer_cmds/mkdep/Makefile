Project = mkdep
Install_Dir = /usr/bin

MANPAGES = mkdep.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) mkdep.gcc.sh $(DSTROOT)/$(Install_Dir)/mkdep
