Project = su

CFILES = su.c
MANPAGES = su.1

Extra_CC_Flags = -Wall -D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip -lpam -lbsm
Extra_LD_Flags += -framework Security

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

Install_Program_Mode = 04555

after_install:
	$(MKDIR) $(DSTROOT)$(ETCDIR)/pam.d
	$(INSTALL_FILE) su.pam $(DSTROOT)$(ETCDIR)/pam.d/su
