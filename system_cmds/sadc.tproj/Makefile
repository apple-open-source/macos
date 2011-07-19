Project = sadc
Install_Dir = /usr/lib/sa

HFILES = sadc.h
CFILES = sadc.c
MANPAGES = sa1.8 sa2.8 sadc.8

OTHERSRCS = Makefile.preamble Makefile Makefile.postamble sa1.8 sa1.sh\
            sa2.8 sa2.sh sadc.8

Extra_CC_Flags = -Wall -Werror
Extra_LD_Flags = -dead_strip

Extra_CC_Flags += -I.
Extra_Frameworks = -framework CoreFoundation -framework IOKit

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

after_install:
	$(INSTALL_DIRECTORY) $(DSTROOT)/private/var/log/sa
	$(INSTALL_DIRECTORY) $(DSTROOT)/$(Install_Dir)
	$(INSTALL_FILE) sa1.sh $(DSTROOT)$(Install_Dir)/sa1
	$(CHMOD) 0555 $(DSTROOT)$(Install_Dir)/sa1
	$(INSTALL_FILE) sa2.sh $(DSTROOT)$(Install_Dir)/sa2
	$(CHMOD) 0555 $(DSTROOT)$(Install_Dir)/sa2
