##
# Makefile for zlib
##

# Project info
Project         = zlib
UserType        = Developer
ToolType        = Libraries
GnuAfterInstall = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Well, not quite, but we're hackers here...

Configure       = ./configure
Configure_Flags = --prefix="$(Install_Prefix)" --shared

lazy_install_source:: shadow_source

Install_Target = install

Version := $(shell sed -n -e '/VERSION "/s/.*"\(.*\)".*/\1/p' < $(Sources)/zlib.h)

strip:
	$(STRIP) -S $(DSTROOT)$(USRLIBDIR)/libz.$(Version).dylib
