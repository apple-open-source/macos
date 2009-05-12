##
# Makefile for cups
##

Project		= cups
UserType	= Administrator
ToolType	= Services

GnuNoChown      = YES
GnuAfterInstall	= post-install install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Tell configure where to find the .order files for the libraries.
Configure_Flags = --with-libcupsorder=/usr/local/lib/OrderFiles/libcups.2.order \
		  --with-libcupsimageorder=/usr/local/lib/OrderFiles/libcupsimage.2.order \
		  --enable-pie \
		  --with-adminkey=system.print.admin \
		  --with-operkey=system.print.admin \
		  --with-archflags="$(RC_CFLAGS)"

# CUPS is able to build 1/2/3/4-way fat on its own, so don't override the
# compiler flags in make, just in configure...
Environment	=

# The "install" target installs...
Install_Target	= install
Install_Flags	=

# Shadow the source tree and force a re-configure as needed
lazy_install_source:: $(BuildDirectory)/Makedefs
	$(_v) if [ -L "$(BuildDirectory)/Makefile" ]; then \
		$(RM) "$(BuildDirectory)/Makefile"; \
		$(CP) "$(Sources)/Makefile" "$(BuildDirectory)/Makefile"; \
	fi

# Re-configure when the configure script or Makedefs template change.
$(BuildDirectory)/Makedefs: $(Sources)/configure $(Sources)/Makedefs.in
	$(_v) $(RM) "$(BuildDirectory)/Makefile"
	$(_v) $(MAKE) shadow_source
	$(_v) $(RM) $(ConfigStamp)

# Strip the installed binaries after install - this allows the B&I scripts
# to extract the symbols.  However, stripping also removes setuid permissions
# so we need to fix the permissions on the lppasswd program.
post-install:
	find $(DSTROOT) -type f -perm +111 -exec $(STRIP) -x '{}' \;
	chmod 4755 $(DSTROOT)/usr/bin/lppasswd

# The plist keeps track of the open source version we ship...
OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE.txt $(OSL)/$(Project).txt
