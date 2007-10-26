##
# Makefile for libxml2
##

# Project info
Project               = libxml2
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --enable-static=no --with-python=no --with-iconv=no --with-icu=yes
Extra_Environment     = LD_TWOLEVEL_NAMESPACE=true
Extra_LD_Flags        = -arch i386 -arch ppc -arch ppc64 -arch x86_64
GnuAfterInstall       = fix-xml2-links install-plist thin-binaries remove-autom4te-cache

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Extract the source.
install_source::
	cd $(SRCROOT)/$(Project) && aclocal && glibtoolize --force && autoconf && automake --add-missing

	# <rdar://problem/5077277>: change library_names_spec to match the ordering in the libxml2 source tarball
	sed -i "" "s/library_names_spec='\\\$${libname}\\\$${release}\\\$${major}\\\$$shared_ext \\\$${libname}\\\$$shared_ext \\\$${libname}\\\$${release}\\\$${versuffix}\\\$$shared_ext'/library_names_spec='\$${libname}\$${release}\$${versuffix}\$$shared_ext \$${libname}\$${release}\$${major}\$$shared_ext \$${libname}\$$shared_ext'/" $(SRCROOT)/$(Project)/configure
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache

	ed - $(Sources)/configure < $(SRCROOT)/patches/add_rc_flags.ed
ifeq ($(shell test -f /usr/local/lib/OrderFiles/libxml2.order && echo yes),yes)
	ed - $(Sources)/configure < $(SRCROOT)/patches/add_sectorder_flags.ed
endif

VERS    = $(shell sw_vers -productVersion)
fix-xml2-links:
	$(RM) $(DSTROOT)/usr/lib/libxml2.2.dylib
	$(RM) $(DSTROOT)/usr/lib/libxml2.dylib
# 10.3 is not found in the version string
ifeq ($(findstring 10.3, $(VERS)),)
	$(MV) $(DSTROOT)/usr/lib/libxml2.2.6.16.dylib $(DSTROOT)/usr/lib/libxml2.2.dylib
else
	$(LN) -s libxml2.2.6.16.dylib $(DSTROOT)/usr/lib/libxml2.2.dylib
endif
	$(LN) -s libxml2.2.dylib $(DSTROOT)/usr/lib/libxml2.dylib
	$(STRIP) -x $(DSTROOT)/usr/lib/libxml2.2.dylib

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/Copyright $(OSL)/$(Project).txt

thin-binaries:
	for binary in xmllint xmlcatalog; do \
		lipo -remove x86_64 -remove ppc64 $(DSTROOT)/usr/bin/$$binary -output $(DSTROOT)/usr/bin/$$binary.thin; \
		$(MV) $(DSTROOT)/usr/bin/$$binary.thin $(DSTROOT)/usr/bin/$$binary; \
	done

remove-autom4te-cache:
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache
