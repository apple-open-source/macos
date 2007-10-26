##
# Makefile for OpenSSH
##
# Wilfredo Sanchez, wsanchez@apple.com
# Disco Vince Giffin, vgiffin@apple.com
##

# Project info
Project               = openssh
ProjectName           = OpenSSH
UserType              = Administrator
ToolType              = Services
Extra_LD_Flags        = -L. -Lopenbsd-compat

Extra_Configure_Flags	= --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5 --without-zlib-version-check --with-4in6 --with-audit=bsm CPPFLAGS="-D__APPLE_SACL__ -D_UTMPX_COMPAT -D__APPLE_UTMPX__ -DUSE_CCAPI -D__APPLE_LAUNCHD__ -D__APPLE_PRIVPTY__" --disable-libutil --disable-utmp --disable-wtmp
Extra_Install_Flags		= sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
GnuAfterInstall			= fixup-dstroot install-startup-item install-plist install-man-pages relocate-sym-files DVG-4730267_disabled_SSH1_by_default DVG-4859983_install_ssh-agent_plist install-strings
ifeq  ($(MACOSX_DEPLOYMENT_TARGET), 10.4)
	Extra_LD_Flags = -L. -Lopenbsd-compat
	Extra_Configure_Flags	= --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5 --without-zlib-version-check --with-4in6 --with-audit=bsm --without-keychain CPPFLAGS="-D__APPLE_SACL__ -DUSE_CCAPI -D__APPLE_GSSAPI_ENABLE__"
	Extra_Install_Flags		= sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
	GnuAfterInstall			= fixup-dstroot install-startup-item install-plist install-man-pages relocate-sym-files DVG-4853931_enable_GSSAPI
else ifeq ($(MACOSX_DEPLOYMENT_TARGET), 10.3)
	Extra_LD_Flags = -L. -Lopenbsd-compat
	Extra_Configure_Flags	= --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5 --without-zlib-version-check --with-4in6 --with-audit=bsm --without-keychain CPPFLAGS="-DUSE_CCAPI -D__APPLE_GSSAPI_ENABLE__"
	Extra_Install_Flags		= sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
	GnuAfterInstall			= fixup-dstroot install-panther-startup-item install-plist install-man-pages relocate-sym-files DVG-4853931_enable_GSSAPI
endif

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags         = DESTDIR=$(DSTROOT)

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 4.5p1
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = bsm.patch pam.patch sacl.patch DVG-4122722+5277818_new_EA.patch DVG-3977221_manpage_tweaks.patch DVG-4212542_auth_error_logging_fix.patch DVG-4157448+4920695_corrected_UsePAM_comment.patch sshpty.c.patch lastlog.patch openssh-4.4p1-gsskex-20061002.patch DVG-4808140_getpwuid_botch.patch DVG-4853931_enable_GSSAPI.patch DVG-4648874_preserve_EA_mtime.patch DVG-4748610+4897588_ssh-agent_via_launchd.patch DVG-4907495_name_resolution_error_message.patch DVG-4694589_16_group_limit_fix.patch DVG-5142987_launchd_DISPLAY_for_X11.patch DVG-5258734_pty_permission_fix.patch AJ-5229538+5383306_keychain.patch DVG+AJ-5370108_fix_globbing_in_Leopard_sftp.patch

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

install-panther-startup-item:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/private/etc/xinetd.d
	$(_v) $(INSTALL_FILE)   -c ssh-via-xinetd  $(DSTROOT)/private/etc/xinetd.d/ssh
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


install-man-pages:
	$(LN) $(DSTROOT)/usr/share/man/man1/ssh-keygen.1 $(DSTROOT)/usr/share/man/man8/sshd-keygen-wrapper.8
	
relocate-sym-files:
	$(CP) $(OBJROOT)/scp $(SYMROOT)/scp
	$(CP) $(OBJROOT)/sftp $(SYMROOT)/sftp
	$(CP) $(OBJROOT)/ssh $(SYMROOT)/ssh
	$(CP) $(OBJROOT)/ssh-add $(SYMROOT)/ssh-add
	$(CP) $(OBJROOT)/ssh-agent $(SYMROOT)/ssh-agent
	$(CP) $(OBJROOT)/ssh-keygen $(SYMROOT)/ssh-keygen
	$(CP) $(OBJROOT)/ssh-keyscan $(SYMROOT)/ssh-keyscan
	$(CP) $(OBJROOT)/ssh-keysign $(SYMROOT)/ssh-keysign
	$(CP) $(OBJROOT)/ssh-rand-helper $(SYMROOT)/ssh-rand-helper
	$(CP) $(OBJROOT)/sshd $(SYMROOT)/sshd

DVG-4730267_disabled_SSH1_by_default:
	sed 's/#Protocol 2,1/Protocol 2/g' $(DSTROOT)/private/etc/sshd_config > $(DSTROOT)/private/etc/sshd_config.temp
	$(MV) $(DSTROOT)/private/etc/sshd_config.temp $(DSTROOT)/private/etc/sshd_config

DVG-4859983_install_ssh-agent_plist:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchAgents
	$(_v) $(INSTALL_FILE) -m 644  -c org.openbsd.ssh-agent.plist $(DSTROOT)/System/Library/LaunchAgents/org.openbsd.ssh-agent.plist
	
DVG-4853931_enable_GSSAPI:
	patch -p0 -d $(DSTROOT) < $(SRCROOT)/patches/DVG-4853931_enable_GSSAPI_AfterInstall.patch

install-strings:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/CoreServices/Resources/English.lproj
	$(_v) $(INSTALL_FILE) -m 644  -c OpenSSH.strings $(DSTROOT)/System/Library/CoreServices/Resources/English.lproj/OpenSSH.strings
