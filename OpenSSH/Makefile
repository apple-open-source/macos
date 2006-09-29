##
# Makefile for OpenSSH
##
# Wilfredo Sanchez, wsanchez@apple.com
##

# Project info
Project               = openssh
ProjectName           = OpenSSH
UserType              = Administrator
ToolType              = Services
Extra_LD_Flags        = -L. -Lopenbsd-compat
Extra_Configure_Flags = --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5 --without-zlib-version-check --with-4in6 --with-audit=bsm CPPFLAGS="-D__APPLE_SACL__" 

Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""

GnuAfterInstall = fixup-dstroot install-startup-item install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags         = DESTDIR=$(DSTROOT)

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.2p1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = kerberos.patch kerb_gssapi_misc.patch pam.patch sacl.patch EA.patch BSM.patch
#AEP_Patches    = bsm.patch apple-bsm.patch kerberos.patch sacl.patch NLS_3995532_configure.patch EA.patch configure.patch NLS_PR-4000739_mindrot_874.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
    AEP_ExtractOption = j
else
    AEP_ExtractOption = z
endif


Install_Target = install-nokeys

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment)

StartupItemDir = $(NSLIBRARYDIR)/StartupItems/SSH

fixup-dstroot:
	$(_v) mkdir -p $(DSTROOT)/private
	$(_v) mv    $(DSTROOT)/etc $(DSTROOT)/private
	$(_v) rmdir $(DSTROOT)/var/empty
	$(_v) rmdir $(DSTROOT)/var

install-startup-item:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchDaemons
	$(_v) $(INSTALL_FILE) -m 644  -c launchd-ssh.plist $(DSTROOT)/System/Library/LaunchDaemons/ssh.plist
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/libexec
	$(_v) $(INSTALL_FILE) -m 555  -c sshd-keygen-wrapper $(DSTROOT)/usr/libexec/sshd-keygen-wrapper

install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
	   echo $$patchfile; \
	   cd $(SRCROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(OSV)/$(ProjectName).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENCE $(OSL)/$(ProjectName).txt
