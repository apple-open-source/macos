##
# Makefile for mod_ssl
##

APXS = $(USRSBINDIR)/apxs-1.3

# Project info
Project               = mod_ssl
ProjectName           = apache_mod_ssl
UserType              = Administration
ToolType              = Services
Extra_Configure_Flags = --with-apxs="$(APXS)" --with-ssl=SYSTEM
Extra_Install_Flags   = APXS="$(APXS) -S LIBEXECDIR=\"$(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)\""
GnuAfterInstall       = strip install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Automatic Extract & Patch

# When upgrading mod_ssl, there are paremeters in the
# apache makefile to update too.

AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 2.8.31-1.3.41
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = NLS_mod_ssl_curent.patch

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
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/mod_ssl_patches/$$patchfile; \
	done
endif


# Well, not really.
Environment    =
Configure      = CFLAGS="$(CC_Archs)" LDFLAGS="$(CC_Archs)" $(BuildDirectory)/configure
Install_Target = install

lazy_install_source:: shadow_source

install::
	@echo "Installing documentation..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(Install_HTML)
	$(_v) $(INSTALL_FILE) -c $(BuildDirectory)/pkg.ssldoc/*.html	\
				 $(BuildDirectory)/pkg.ssldoc/*.gif	\
				 $(BuildDirectory)/pkg.ssldoc/*.jpg	\
				 $(DSTROOT)$(Install_HTML)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt

strip:
	$(_v) strip -S $(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)/libssl.so
