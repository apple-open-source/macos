##
# Makefile for zlib
##

# Project info
Project           = zlib
UserType          = Developer
ToolType          = Libraries
GnuAfterInstall   = install-strip install-plist install-old-symlink
SDKROOT          ?= /
Extra_CC_Flags    = -fPIC -DUSE_MMAP -isysroot $(SDKROOT) -dead_strip
Extra_LD_Flags    = -L. -lz

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Well, not quite, but we're hackers here...
Configure_Flags = --prefix=$(Install_Prefix) --shared
Install_Target  = install

install-strip:
	$(STRIP) -S $(DSTROOT)$(USRLIBDIR)/libz.1.dylib

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(TAIL) -n 33 $(OBJROOT)/README > $(OSL)/$(Project).txt

install-old-symlink:
	$(LN) -fs libz.1.dylib $(DSTROOT)/usr/lib/libz.1.1.3.dylib
