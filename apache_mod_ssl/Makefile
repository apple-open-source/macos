##
# Makefile for mod_ssl
##

APXS = $(USRSBINDIR)/apxs

# Project info
Project               = mod_ssl
ProjectName           = apache_mod_ssl
UserType              = Administration
ToolType              = Services
Extra_Configure_Flags = --with-apxs="$(APXS)" --with-ssl=SYSTEM
Extra_Install_Flags   = APXS="$(APXS) -S LIBEXECDIR=\"$(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)\""
GnuAfterInstall       = strip

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

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

strip:
	$(_v) strip -S $(DSTROOT)$(shell $(APXS) -q LIBEXECDIR)/libssl.so
