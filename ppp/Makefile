##
# Makefile for ppp
##

# Project info
Project             = ppp
UserType            = Developer
ToolType            = Extensions
Extra_Install_Flags = DSTROOT="$(DSTROOT)"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

lazy_install_source:: shadow_source

Environment = ARCHFLAGS="$(CFLAGS)"

Install_Target = install

clean = clean



