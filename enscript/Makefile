##
# Makefile for enscript
##

# Project info
Project               = enscript
UserType              = Administration
ToolType              = Commands
Extra_Configure_Flags = --sysconfdir=$(Install_Prefix)/share/enscript --with-media=Letter
Extra_Install_Flags   = sysconfdir=$(DSTROOT)$(Install_Prefix)/share/enscript

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
