Project = mkdep
Install_Dir = /usr/bin

MANPAGES = mkdep.1

Extra_CC_Flags = -mdynamic-no-pic -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(INSTALL_SCRIPT) mkdep.gcc.sh $(DSTROOT)/$(Install_Dir)/mkdep
