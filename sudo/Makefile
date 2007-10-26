##
# Makefile for sudo
##

# Project info
Project               = sudo
UserType              = Administrator
ToolType              = Commands
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)"
Extra_CC_Flags        = -mdynamic-no-pic
GnuAfterInstall       = securityserver_workaround_3273205 install-plist

Extra_Configure_Flags = --with-password-timeout=0 --disable-setreuid --with-env-editor --with-pam --with-libraries=bsm --with-noexec=no --sysconfdir="$(ETCDIR)" --with-timedir="/var/db/sudo" CPPFLAGS="-D__APPLE_MEMBERD__"
ifeq  ($(MACOSX_DEPLOYMENT_TARGET), 10.4)
	Extra_Configure_Flags = --with-password-timeout=0 --disable-setreuid --with-env-editor --with-pam --with-libraries=bsm --with-noexec=no --sysconfdir="$(ETCDIR)" --with-timedir="/var/db/sudo" CPPFLAGS="-D__APPLE_MEMBERD__"
else ifeq ($(MACOSX_DEPLOYMENT_TARGET), 10.3)
	Extra_Configure_Flags = --with-password-timeout=0 --disable-setreuid --with-env-editor --with-pam --with-libraries=bsm --with-noexec=no --sysconfdir="$(ETCDIR)" --with-timedir="/var/db/sudo"
endif

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

securityserver_workaround_3273205:
	chmod 4511 "$(DSTROOT)/usr/bin/sudo"

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.6.8p12
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = patch-Makefile.in \
                 patch-auth__sudo_auth.c \
                 patch-sudo.c \
                 patch-sudo.man.in \
                 patch-sudoers \
                 patch-sudoers.man.in \
                 DVG-4724013_manpage_tweaks.patch \
                 DVG-4646431_password_after_reboot.patch \
                 DVG-4594036+4873886_new_warning.patch \
                 DVG-4130827_memberd_group_resolution.patch \
                 DVG-5095270+5159283_clean_environment.patch

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
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE $(OSL)/$(Project).txt
