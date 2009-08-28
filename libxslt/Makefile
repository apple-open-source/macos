##
# Makefile for libxslt
##

# Project info
Project               = libxslt
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --without-python --disable-static
Extra_Environment     = LD_TWOLEVEL_NAMESPACE=1 
Extra_LD_Flags        = 
GnuAfterInstall       = extract-symbols fix-xslt-links fix-exslt-links install-plist remove-autom4te-cache

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Extract the source.
install_source::
	cd $(SRCROOT)/$(Project) && aclocal && glibtoolize --force && autoconf && automake --add-missing
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache
	ln -f $(SRCROOT)/$(Project)/Copyright $(SRCROOT)/$(Project)/COPYING

install_headers:: shadow_source configure
	$(MAKE) -C $(BuildDirectory)/libxslt $(Environment) $(Install_Flags) install-xsltincHEADERS
	$(MAKE) -C $(BuildDirectory)/libexslt $(Environment) $(Install_Flags) install-exsltincHEADERS

extract-symbols:
	for binary in xsltproc libxslt.1.dylib libexslt.0.dylib; do \
		$(CP) $$(find $(OBJROOT) -path "*/.libs/$${binary}") $(SYMROOT)/; \
		dsymutil $(SYMROOT)/$${binary}; \
	done

fix-xslt-links:
	$(RM) $(DSTROOT)/usr/lib/libxslt.1.1.??.dylib
	$(RM) $(DSTROOT)/usr/lib/libxslt.dylib
	$(LN) -s libxslt.1.dylib $(DSTROOT)/usr/lib/libxslt.dylib

fix-exslt-links:
	$(RM) $(DSTROOT)/usr/lib/libexslt.0.8.??.dylib
	$(RM) $(DSTROOT)/usr/lib/libexslt.dylib
	$(LN) -s libexslt.0.dylib $(DSTROOT)/usr/lib/libexslt.dylib

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/Copyright $(OSL)/$(Project).txt

remove-autom4te-cache:
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache
	ln -f $(SRCROOT)/$(Project)/Copyright $(SRCROOT)/$(Project)/COPYING
