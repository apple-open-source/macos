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
Extra_Configure_Flags = --sysconfdir="/etc" --disable-suid-ssh --with-ssl-dir=/usr/include/openssl --with-random=/dev/urandom
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)" MANPAGES=""
Extra_Environment     = AR="$(SRCROOT)/ar.sh"

GnuAfterInstall = install-startup-item

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install-nokeys

build::
	$(_v) $(MAKE) -C $(BuildDirectory) $(Environment)

StartupItemDir = $(NSLIBRARYDIR)/StartupItems/SSH

install-startup-item:
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(StartupItemDir)/Resources/English.lproj
	$(_v) $(INSTALL_SCRIPT) -c startup.script $(DSTROOT)$(StartupItemDir)/SSH
	$(_v) $(INSTALL_FILE)   -c startup.plist  $(DSTROOT)$(StartupItemDir)/StartupParameters.plist
	$(_v) $(INSTALL_FILE)   -c Localizable.strings  $(DSTROOT)$(StartupItemDir)/Resources/English.lproj/Localizable.strings
