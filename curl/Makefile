##
# Makefile for curl
##

# Project info
Project         = curl
UserType        = Developer
ToolType        = Commands
Extra_CC_Flags  = -fno-common
GnuAfterInstall = install-strip

# Don't ship libcurl (not supported)
Extra_Environment   = AR='$(SRCROOT)/ar.sh' 

# It's a Common Source project
#

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
lazy_install_source:: shadow_source

install-strip:
	strip -x $(DSTROOT)/usr/lib/libcurl.2.dylib
	strip -S "$(DSTROOT)/usr/lib/libcurl.a"
	rm -f "$(DSTROOT)/usr/lib/libcurl.la"
	ln -s libcurl.2.dylib $(DSTROOT)/usr/lib/libcurl.2.0.2.dylib
