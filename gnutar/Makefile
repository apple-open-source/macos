##
# Makefile for tar
##

# Project info
Project               = tar
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu"
GnuAfterInstall       = install-man
Extra_Environment     = STRIP='strip'

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-man:
	mkdir -p "$(DSTROOT)/usr/share/man/man1"
	install -c -m 644 "$(SRCROOT)/gnutar.1" "$(DSTROOT)/usr/share/man/man1/gnutar.1"
	ln -f "$(DSTROOT)/usr/bin/gnutar" "$(DSTROOT)/usr/bin/tar" 
	ln -f "$(DSTROOT)/usr/share/man/man1/gnutar.1" "$(DSTROOT)/usr/share/man/man1/tar.1"
	rm -f "$(DSTROOT)/usr/lib/charset.alias"
	rm -f "$(DSTROOT)/usr/share/info/dir"
	rm -f "$(DSTROOT)/usr/share/locale/locale.alias"
	mkdir -p "$(DSTROOT)/private/etc"
	ln -s /usr/sbin/rmt "$(DSTROOT)/private/etc"
