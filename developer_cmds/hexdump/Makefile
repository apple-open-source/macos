Project = hexdump
Install_Dir = /usr/bin

HFILES = hexdump.h
CFILES = conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
MANPAGES = hexdump.1 od.1

Extra_CC_Flags = -mdynamic-no-pic -dead_strip \
	-D__FBSDID=__RCSID

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	$(LN) -f $(DSTROOT)/$(Install_Dir)/hexdump $(DSTROOT)$(Install_Dir)/od
