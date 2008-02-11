Project = id
Install_Dir = /usr/bin

CFILES = id.c
MANPAGES = groups.1 id.1 whoami.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID -DUSE_BSM_AUDIT \
	-Wno-error
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) $(DSTROOT)/$(Install_Dir)/id $(DSTROOT)/$(Install_Dir)/groups
	$(LN) $(DSTROOT)/$(Install_Dir)/id $(DSTROOT)/$(Install_Dir)/whoami
