##
# Makefile for CVS
##

# Project info
Project               = cvs
Extra_Configure_Flags = --with-gssapi
UserType              = Developer
ToolType              = Commands
GnuAfterInstall	      = install-man-pages

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

MANPAGES = man/rcs2log.1 

install-man-pages:
	install -d $(DSTROOT)/usr/share/man/man1
	install -c -m 444 $(SRCROOT)/$(MANPAGES) $(DSTROOT)/usr/share/man/man1/
