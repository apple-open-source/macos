# Common Makefile
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

TZDIR:=$(shell sed -n '/define[[:space:]]*TZDIR/s/[^"]*"\([^"]*\)".*/\1/p' $(SDKROOT)/usr/include/tzfile.h)

# 13121743: We need to modify bsd.sb depending on the value of TZDIR for the
# current platform, but we don't need a copy of bsd.sb in /usr/share/sandbox.
# So we make a modified copy in $(SYMROOT), and the top-level Makefile can make
# copies of it where they are needed.
install::
	sed 's,TO-BE-REPLACED-WITH-TZDIR,$(TZDIR),' $(SRCROOT)/usr/share/sandbox/bsd.sb >$(SYMROOT)/bsd.sb
