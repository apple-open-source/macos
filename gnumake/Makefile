##
# Makefile for make
##

# Project info
Project               = make
ProjectName           = gnumake
UserType              = Developer
ToolType              = Commands
Extra_Configure_Flags = --program-prefix="gnu" --disable-nls
GnuAfterInstall       = install-links

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-links:
	@echo "Installing make symlink"
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRBINDIR)
	$(_v) $(LN) -fs gnumake $(DSTROOT)$(USRBINDIR)/make
	$(_v) $(LN) -f $(DSTROOT)/usr/share/man/man1/gnumake.1 $(DSTROOT)/usr/share/man/man1/make.1
