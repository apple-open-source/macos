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
Extra_Environment   = includedir="/usr/local/include"			\
		      libdir="/usr/local/lib"				\
		      man3dir="/usr/local/share/man/man3"		\
		      man5dir="/usr/local/share/man/man5"
Extra_Install_Flags = includedir="$(DSTROOT)/usr/include"		\
		      libdir="$(DSTROOT)/usr/lib"			\
		      man3dir="$(DSTROOT)/usr/share/man/man3"	\
		      man5dir="$(DSTROOT)/usr/share/man/man5"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

install-strip:
	strip -S "$(DSTROOT)/usr/lib/libcurl.a"
	rm -f "$(DSTROOT)/usr/lib/libcurl.la"
