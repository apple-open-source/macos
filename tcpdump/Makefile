##
# Makefile for tcpdump
##

# Project info
Project         = tcpdump
UserType        = Developer
ToolType        = Commands
GnuAfterInstall = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Extra_CC_Flags = -DHAVE_CONFIG_H -I.

lazy_install_source:: shadow_source

Install_Target = install

strip:
	$(STRIP) -x -S $(DSTROOT)/$(Install_Prefix)/sbin/tcpdump
