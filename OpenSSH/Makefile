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
Extra_CC_Flags        = -no-cpp-precomp -Dcrc32=crcsum32 # crc32() symbol conflicts with zlib (!)
Extra_LD_Flags        = -L. -Lopenbsd-compat
Extra_Configure_Flags = --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom --with-tcp-wrappers --with-pam --with-kerberos5
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
Extra_Environment     = AR="$(SRCROOT)/ar.sh"

GnuAfterInstall = fixup-dstroot install-startup-item

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags         = DESTDIR=$(DSTROOT)

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
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/private/etc/xinetd.d
	$(_v) $(INSTALL_FILE)   -c ssh-via-xinetd  $(DSTROOT)/private/etc/xinetd.d/ssh
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/usr/libexec
	$(_v) $(INSTALL_FILE) -m 555  -c sshd-keygen-wrapper $(DSTROOT)/usr/libexec/sshd-keygen-wrapper
