#
# XBS-compatible Makefile for automake.
#

Project         = automake
UserType        = Developer
ToolType        = Commands
GnuAfterInstall = remove-dir

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir
