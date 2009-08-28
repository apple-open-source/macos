Project = unifdef
Install_Dir = /usr/bin

CFILES = unifdef.c
MANPAGES = unifdef.1

Extra_CC_Flags = -Wall -Werror -mdynamic-no-pic
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)/usr/share/man/man1/unifdef.1 $(DSTROOT)/usr/share/man/man1/unifdefall.1
	$(INSTALL_SCRIPT) unifdefall.sh $(DSTROOT)$(Install_Dir)/unifdefall
