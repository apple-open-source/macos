Project = ed
Install_Dir = /bin

CFILES = buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c
MANPAGES = ed.1 red.1

Extra_CC_Flags = -Wall -mdynamic-no-pic \
        -D__FBSDID=__RCSID \
	-I$(SDKROOT)/System/Library/Frameworks/System.framework/PrivateHeaders \
	-D__DARWIN_UNIX03=1
Extra_LD_Flags = -dead_strip

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
