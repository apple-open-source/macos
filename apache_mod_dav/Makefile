##
# Makefile for mod_dav
##

APXS = /usr/sbin/apxs

# Project info
Project               = mod_dav
ProjectName	      = apache_mod_dav
UserType              = Administrator
ToolType              = Services
Extra_Configure_Flags = --with-apxs
Extra_CC_Flags        = -I.
Extra_Install_Flags   = INSTALL_ROOT="$(DSTROOT)"
GnuAfterInstall       = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

strip:
	$(_v) $(STRIP) -S $(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)/libdav.so
