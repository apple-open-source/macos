##
# Makefile for tar
##

# Project info
Project               = tar
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu"
GnuAfterInstall       = install-man

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-man:
	mkdir -p "$(DSTROOT)/usr/share/man/man1"
	install -c -m 644 "$(SRCROOT)/gnutar.1" "$(DSTROOT)/usr/share/man/man1/gnutar.1"
