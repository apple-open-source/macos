##
# Makefile for less
##

# Project info
Project  = less
UserType = Administration
ToolType = Commands
GnuAfterInstall = link install-man

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

link:
	$(LN) -f $(DSTROOT)/usr/bin/less $(DSTROOT)/usr/bin/more
	$(LN) -f $(DSTROOT)/usr/share/man/man1/less.1 $(DSTROOT)/usr/share/man/man1/more.1

# We install GNU Debian's lessecho.1 man page, because less does not come with one.
install-man:
	install -d $(DSTROOT)/usr/share/man/man1
	install -m 444 $(SRCROOT)/lessecho.1 $(DSTROOT)/usr/share/man/man1/lessecho.1
