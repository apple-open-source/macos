Project = chown
Install_Dir = /usr/sbin

CFILES = chown.c
MANPAGES = chgrp.1 chown.8

Extra_CC_Flags = -Wall -mdynamic-no-pic \
	-D__FBSDID=__RCSID
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)$(Install_Dir)/chown \
		$(DSTROOT)/usr/bin/chgrp
