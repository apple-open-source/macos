##
# Makefile for zsh
##

# Project info
Project               = zsh
UserType              = Administration
ToolType              = Commands
Extra_CC_Flags        = -no-cpp-precomp
Extra_Configure_Flags = --bindir="$(BINDIR)"
Extra_Install_Flags   = bindir="$(DSTROOT)$(BINDIR)"
GnuAfterInstall	      = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

strip:
	find $(DSTROOT) -type f -perm +111 -exec strip -x '{}' \;
