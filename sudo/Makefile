##
# Makefile for sudo
##
# Wilfredo Sanchez | wsanchez@apple.com
##

# Project info
Project               = sudo
UserType              = Administrator
ToolType              = Commands
Extra_Configure_Flags = --with-password-timeout=0 --disable-setreuid --with-env-editor --with-pam --with-libraries=bsm
Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)"
GnuAfterInstall       = stupid_securityserver_workaround_HACK_3273205

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

stupid_securityserver_workaround_HACK_3273205:
	chmod 4511 "$(DSTROOT)/usr/bin/sudo"
