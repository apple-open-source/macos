##
# Makefile for sudo
##
# Wilfredo Sanchez | wsanchez@apple.com
##

# Project info
Project               = sudo
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --with-password-timeout=0 --disable-setreuid --with-env-editor 
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install
