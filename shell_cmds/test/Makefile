Project = test
Install_Dir = /bin

CFILES = test.c
MANPAGES = test.1

Extra_CC_Flags = -Wall -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
include $(CoreOSMakefiles)/ProjectBuilder/Makefile.Postamble.Common

after_install:
	$(LN) -f $(DSTROOT)/$(Install_Dir)/test $(DSTROOT)$(Install_Dir)/[
	$(LN) -f $(DSTROOT)/usr/share/man/man1/test.1 \
		$(DSTROOT)/usr/share/man/man1/[.1
