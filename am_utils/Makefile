##
# Makefile for am-utils
##
# Jordan K. Hubbard <jkh@apple.com>
##

# Project info
Project         = am-utils
UserType        = Administration
ToolType        = Services
Extra_CC_Flags  = -no-cpp-precomp
GnuAfterInstall = do-fixups

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Clean up various weirdness to conform to Mac OS X standards
do-fixups:
	mkdir -p $(DSTROOT)/usr/share
	mv $(DSTROOT)/usr/etc $(DSTROOT)/usr/share/amd
	rm -f $(DSTROOT)/usr/share/info/dir
	rm -rf $(DSTROOT)/usr/lib
