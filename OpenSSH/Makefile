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


have_tconf =: $(strip $(shell which tconf))
ifneq ($(have_tconf),)
	Product=$(shell tconf --product)
	Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
endif


Environment 		= CC="/usr/bin/llvm-gcc-4.2"
Extra_CC_Flags          = -fPIE -D_FORTIFY_SOURCE=2
Extra_LD_Flags          = -L. -Lopenbsd-compat -Wl,-pie -framework CoreFoundation -framework OpenDirectory -lresolv
Extra_Configure_Flags	= --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5 --without-zlib-version-check --with-4in6 --with-audit=bsm CPPFLAGS="-D__APPLE_SACL__ -D_UTMPX_COMPAT -D__APPLE_UTMPX__ -DUSE_CCAPI -D__APPLE_LAUNCHD__ -D__APPLE_MEMBERSHIP__ -D__APPLE_SANDBOX_PRIVSEP_CHILDREN__ -D__APPLE_CROSS_REALM__ -D__APPLE_XSAN__" --with-keychain=apple --disable-libutil --disable-utmp --disable-wtmp --with-privsep-user=_sshd
Extra_Install_Flags		= sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
GnuAfterInstall			= fixup-dstroot install-startup-item install-plist install-man-pages install-sandbox relocate-sym-files DVG-4859983_install_ssh-agent_plist install-strings install-PAM-config-files

ifeq ($(Embedded), YES)
	Extra_CC_Flags          =
	Extra_LD_Flags          = -L. -Lopenbsd-compat
	Extra_Configure_Flags	= --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir="$(SDKROOT)/usr/include/openssl" --with-random=/dev/urandom --without-zlib-version-check --with-4in6 CPPFLAGS="-D_UTMPX_COMPAT -D__APPLE_UTMPX__ -D__APPLE_LAUNCHD__ -D__APPLE_SANDBOX_PRIVSEP_CHILDREN__" --disable-libutil --disable-utmp --disable-wtmp --with-keychain=no --host=none-apple-darwin --with-privsep-user=_sshd
	Extra_Environment       = ac_cv_header_endian_h=no
	Extra_Install_Flags		= sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
	GnuAfterInstall			= fixup-dstroot install-startup-item install-plist install-man-pages install-sandbox relocate-sym-files fix-startup-item-for-embedded
endif

ifdef SDKROOT
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags         = DESTDIR=$(DSTROOT)

Install_Target = install-nokeys

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment)
	$(_v) cp sshd-keygen-wrapper $(OBJROOT)/sshd-keygen-wrapper
ifeq "$(Embedded)" "YES"
	$(_v) sed -i '' 's/\/etc\//\/var\/db\//g' $(OBJROOT)/sshd-keygen-wrapper
	$(_v) sed -i '' 's/\#HostKey \/etc\/ssh_host_/HostKey \/var\/db\/ssh_host_/g' $(OBJROOT)/sshd_config.out
endif

StartupItemDir = $(NSLIBRARYDIR)/StartupItems/SSH

fixup-dstroot:
	$(_v) mkdir -p $(DSTROOT)/private
	$(_v) mv    $(DSTROOT)/etc $(DSTROOT)/private
	$(_v) rmdir $(DSTROOT)/var/empty
	$(_v) rmdir $(DSTROOT)/var
ifeq "$(Embedded)" "YES"
	$(_v) rm $(DSTROOT)/usr/libexec/ssh-keysign
endif

install-startup-item:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchDaemons
	$(_v) $(INSTALL_FILE) -m 644  -c launchd-ssh.plist $(DSTROOT)/System/Library/LaunchDaemons/ssh.plist
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/libexec
	$(_v) $(INSTALL_FILE) -m 555  -c $(OBJROOT)/sshd-keygen-wrapper $(DSTROOT)/usr/libexec/sshd-keygen-wrapper

fix-startup-item-for-embedded: install-startup-item
ifeq ($(Embedded), YES)
	/usr/libexec/PlistBuddy -x \
		-c "Delete :Disabled" \
		-c "Delete :SessionCreate" \
		-c "Add :Sockets:Listeners:SockFamily string IPv4" \
		"$(DSTROOT)/System/Library/LaunchDaemons/ssh.plist"
ifeq ($(Product), iPhone)
	/usr/libexec/PlistBuddy -x \
		-c 'Delete :Sockets:Listeners:Bonjour' \
		-c "Add :Sockets:Listeners:Bonjour bool false" \
		-c "Add :Sockets:Listeners:SockNodeName string localhost" \
		"$(DSTROOT)/System/Library/LaunchDaemons/ssh.plist"
endif
endif


install_source::

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

install-sandbox:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/share/sandbox
	$(_v) $(INSTALL_FILE) -o root -g wheel -m 644 -c sshd.sb $(DSTROOT)/usr/share/sandbox/sshd.sb

DVG-4859983_install_ssh-agent_plist:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/LaunchAgents
	$(_v) $(INSTALL_FILE) -m 644  -c org.openbsd.ssh-agent.plist $(DSTROOT)/System/Library/LaunchAgents/org.openbsd.ssh-agent.plist

install-strings:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/System/Library/CoreServices/Resources/English.lproj
	$(_v) $(INSTALL_FILE) -m 644  -c OpenSSH.strings $(DSTROOT)/System/Library/CoreServices/Resources/English.lproj/OpenSSH.strings

install-PAM-config-files:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/private/etc/pam.d
	$(_v) $(INSTALL_FILE) -m 444  -c $(SRCROOT)/pam.d/sshd $(DSTROOT)/private/etc/pam.d/sshd
