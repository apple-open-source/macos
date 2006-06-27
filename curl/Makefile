##
# Makefile for curl
##

# Project info
Project           = curl
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = install-fixup install-plist compat-symlink move-static

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 7.13.1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = configure.diff curl-config.in.diff lib__ssluse.c.diff \
                 libcurl-ntlmbuf.patch disable_poll.patch \
                 libcurl-urllen.patch

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

install-fixup:
	$(RM) $(DSTROOT)/usr/lib/libcurl.la
	$(STRIP) -S $(DSTROOT)/usr/lib/libcurl.a
	$(RM) $(DSTROOT)/usr/lib/libcurl.dylib $(DSTROOT)/usr/lib/libcurl.3.dylib
	$(MV) $(DSTROOT)/usr/lib/libcurl.3.0.0.dylib $(DSTROOT)/usr/lib/libcurl.3.dylib
	$(LN) -fs libcurl.3.dylib $(DSTROOT)/usr/lib/libcurl.dylib
	$(LN) -fs libcurl.3.dylib $(DSTROOT)/usr/lib/libcurl.3.0.0.dylib

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/COPYING $(OSL)/$(Project).txt

compat-symlink:
	$(LN) -s libcurl.3.dylib $(DSTROOT)/usr/lib/libcurl.2.dylib

move-static:
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libcurl.a $(DSTROOT)/usr/local/lib
