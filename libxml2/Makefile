##
# Makefile for libxslt
##

# Project info
Project               = libxml2
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --enable-static=no --with-python=no
Extra_Environment     = LD_TWOLEVEL_NAMESPACE=true
Extra_LD_Flags        = -arch i386 -arch ppc
GnuAfterInstall       = fix-xml2-links install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.6.16
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = xmlversion.h.diff xmlversion.h.in.diff Makefile.in.diff xmlunicode.c.diff chvalid.c.diff HTMLparser.c.diff HTMLtree.c.diff legacy.c.diff parser.c.diff xmlIO.c.diff xmlwriter.c.diff xpath.c.diff

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

	ed - $(Sources)/configure < $(SRCROOT)/patches/add_rc_flags.ed
ifeq ($(shell test -f /usr/local/lib/OrderFiles/libxml2.order && echo yes),yes)
	ed - $(Sources)/configure < $(SRCROOT)/patches/add_sectorder_flags.ed
endif

VERS    = $(shell sw_vers -productVersion)
fix-xml2-links:
	$(RM) $(DSTROOT)/usr/lib/libxml2.2.dylib
	$(RM) $(DSTROOT)/usr/lib/libxml2.dylib
# 10.3 is not found in the version string
ifeq ($(findstring 10.3, $(VERS)),)
	$(MV) $(DSTROOT)/usr/lib/libxml2.2.6.16.dylib $(DSTROOT)/usr/lib/libxml2.2.dylib
else
	$(LN) -s libxml2.2.6.16.dylib $(DSTROOT)/usr/lib/libxml2.2.dylib
endif
	$(LN) -s libxml2.2.dylib $(DSTROOT)/usr/lib/libxml2.dylib

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/Copyright $(OSL)/$(Project).txt
