Project = arch
Install_Dir = /usr/bin

MFILES = arch.m
Extra_Frameworks = -framework Foundation

MANPAGES = arch.1 machine.1

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -DARCH_PROG=\"arch\" -DMACHINE_PROG=\"machine\"

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

HELPERDIR = /AppleInternal/Developer/Tools
after_install:
	@-$(RM) -f $(DSTROOT)$(INSTALLDIR)/machine
	$(LN) -fh $(DSTROOT)$(Install_Dir)/arch $(DSTROOT)$(Install_Dir)/machine
	$(INSTALL_DIRECTORY) -d $(DSTROOT)$(HELPERDIR)
	$(INSTALL_SCRIPT) arch_helper.pl $(DSTROOT)$(HELPERDIR)
