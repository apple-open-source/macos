##
# Makefile for screen
##

# Project info
Project               = screen
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall        = install-strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

install-strip:
	strip $(DSTROOT)/usr/bin/screen-*
	rm $(DSTROOT)/usr/bin/screen
	mv $(DSTROOT)/usr/bin/screen-* $(DSTROOT)/usr/bin/screen
	rm $(DSTROOT)/usr/share/info/dir
