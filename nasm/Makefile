##
# nasm Makefile
##

# Project info
Project               = nasm
UserType              = Developer
ToolType              = Commands
Install_Prefix	      = /usr
Extra_Configure_Flags =
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = post-install install-plist

# It's a GNU Source project
include ./GNUSource.make

Install_Target = install_everything

# Extract the source.
install_source::
	ditto . $(SRCROOT)

# Move things to where they were supposed to be.
post-install:
	$(MKDIR) -p $(DSTROOT)/usr/bin
	$(STRIP) $(DSTROOT)/usr/bin/nasm \
		-o $(DSTROOT)/usr/bin/nasm
	$(STRIP) $(DSTROOT)/usr/bin/ndisasm \
		-o $(DSTROOT)/usr/bin/ndisasm
	$(MKDIR) -p $(DSTROOT)/usr/share/man/man1
	$(MV) $(DSTROOT)/usr/share/man/man1/nasm.1 \
		$(DSTROOT)/usr/share/man/man1/nasm.1

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
