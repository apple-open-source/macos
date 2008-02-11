##
# Makefile for cups
##

Project		= cups
UserType	= Administrator
ToolType	= Services

Extra_CC_Flags	= -I..
Extra_LD_Flags	= -L$(OBJROOT)/cups -L$(OBJROOT)/filter
GnuNoChown      = YES
GnuAfterInstall	= post-install install-plist
Extra_Environment = ARCHFLAGS="$(RC_CFLAGS)"

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Well, not really but we can make it work...
Install_Target	= install

# Tell configure where to find the .order files for the libraries.
Configure_Flags = --with-libcupsorder=/usr/local/lib/OrderFiles/libcups.2.order \
		  --with-libcupsimageorder=/usr/local/lib/OrderFiles/libcupsimage.2.order

# Shadow the source tree
lazy_install_source:: shadow_source
	$(_v) if [ -L $(BuildDirectory)/Makefile ]; then						\
		 $(RM) "$(BuildDirectory)/Makefile";						\
		 $(CP) "$(Sources)/Makefile" "$(BuildDirectory)/Makefile";			\
	      fi

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

# Remove the 64-bit slices from everything but the libraries and remove
# unneeded symbols...
#
# Note: The CUPS build system is smart enough to only build the libraries
#       64-bit, however the B&I system overrides all of the CUPS settings
#       and builds all apps 4-way fat anyways.  So, we still need to use
#       lipo...
post-install:
	@for i in `find $(DSTROOT) -type f -perm +111 ! -name "libcups*.dylib" -a ! -name "phpcups.so"`; do \
		if $(LIPO) -info $$i | $(GREP) -qe ppc64 -e x86_64; then \
			echo "Removing 64-bit architectures from $$i"; \
			$(LIPO) -remove ppc64 -remove x86_64 $$i -output $$i.thin && $(MV) $$i.thin $$i; \
		fi \
	done
	find $(DSTROOT) -type f -perm +111 -exec $(STRIP) -x '{}' \;
	chown root:lp $(DSTROOT)/usr/bin/lppasswd
	chmod 4755 $(DSTROOT)/usr/bin/lppasswd

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/LICENSE.txt $(OSL)/$(Project).txt
