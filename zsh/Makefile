##
# Makefile for zsh
##

# Project info
Project               = zsh
UserType              = Administration
ToolType              = Commands
GnuAfterInstall	      = install-links
Extra_CC_Flags        = -traditional-cpp
Extra_Configure_Flags = --bindir="$(BINDIR)"
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-links:
	$(_v) $(INSTALL_PROGRAM) -c $(DSTROOT)$(BINDIR)/zsh $(DSTROOT)$(BINDIR)/sh
