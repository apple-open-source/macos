##
# Makefile for screen
##

# Project info
Project               = screen
UserType              = Administrator
ToolType              = Commands
GnuAfterInstall       = install-strip
Extra_Configure_Flags = --with-sys-screenrc=$(ETCDIR)/screenrc

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
# Function prototype detection in osdef.sh quitely breaks
# with cpp-precomp enabled, causing a later build failure.
Environment += CPPFLAGS=-no-cpp-precomp

Install_Flags = DESTDIR=$(DSTROOT)

install-strip:
	strip $(DSTROOT)/usr/bin/screen-*
	rm $(DSTROOT)/usr/bin/screen
	mv $(DSTROOT)/usr/bin/screen-* $(DSTROOT)/usr/bin/screen
	rm $(DSTROOT)/usr/share/info/dir
