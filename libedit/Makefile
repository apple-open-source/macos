##
# Makefile for libedit
##

Project             = libedit
Extra_Install_Flags = PREFIX=$(DSTROOT)$(Install_Prefix)
GnuAfterInstall     = install-plist fix-install

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target      = install

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(HEAD) $(Sources)/el.c | $(SED) 1,2d > $(OSL)/$(Project).txt

fix-install:
	$(MKDIR) $(DSTROOT)/usr/share
	$(MV) $(DSTROOT)/usr/man $(DSTROOT)/usr/share
	$(MKDIR) $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libedit.a $(DSTROOT)/usr/local/lib
	$(MV) $(DSTROOT)/usr/lib/libreadline.a $(DSTROOT)/usr/local/lib
	$(STRIP) -x $(DSTROOT)/usr/lib/libedit.2.dylib

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.6.9
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in \
                 patch-configure

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

install_headers:: shadow_source configure
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $(Install_Flags) install_hdr
