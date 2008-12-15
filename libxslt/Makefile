##
# Makefile for libxslt
##

# Project info
Project               = libxslt
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --without-python --disable-static
Extra_Environment     = LD_TWOLEVEL_NAMESPACE=1 
Extra_LD_Flags        = 
GnuAfterInstall       = fix-xslt-links fix-exslt-links install-plist thin-binaries

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.1.12
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff ltmain.sh.diff Makefile.in.diff rdar-5865376.patch

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

install_headers:: shadow_source configure
	$(MAKE) -C $(BuildDirectory)/libxslt $(Environment) $(Install_Flags) install-xsltincHEADERS
	$(MAKE) -C $(BuildDirectory)/libexslt $(Environment) $(Install_Flags) install-exsltincHEADERS

fix-xslt-links:
	$(RM) $(DSTROOT)/usr/lib/libxslt.1.dylib
	$(MV) $(DSTROOT)/usr/lib/libxslt.1.1.12.dylib $(DSTROOT)/usr/lib/libxslt.1.dylib
	$(RM) $(DSTROOT)/usr/lib/libxslt.dylib
	$(LN) -s libxslt.1.dylib $(DSTROOT)/usr/lib/libxslt.dylib

fix-exslt-links:
	$(RM) $(DSTROOT)/usr/lib/libexslt.0.dylib
	$(MV) $(DSTROOT)/usr/lib/libexslt.0.8.10.dylib $(DSTROOT)/usr/lib/libexslt.0.dylib
	$(RM) $(DSTROOT)/usr/lib/libexslt.dylib
	$(LN) -s libexslt.0.dylib $(DSTROOT)/usr/lib/libexslt.dylib

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/Copyright $(OSL)/$(Project).txt

thin-binaries:
	for binary in xsltproc; do \
		lipo -remove x86_64 -remove ppc64 $(DSTROOT)/usr/bin/$$binary -output $(DSTROOT)/usr/bin/$$binary.thin; \
		$(MV) $(DSTROOT)/usr/bin/$$binary.thin $(DSTROOT)/usr/bin/$$binary; \
	done
