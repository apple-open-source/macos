Project        = swig
ProjectVersion = 1.3.40
Patches        = Makefile.in.diff \
                 Source__Makefile.in.diff

INSTALL_PREFIX = /usr/local

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@for file in $(Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$file) || exit 1; \
	done

install::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure --disable-dependency-tracking --prefix=$(INSTALL_PREFIX) --without-maximum-compile-warnings --disable-ccache
	$(MAKE) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)
	$(INSTALL_FILE) $(OBJROOT)/Lib/swigwarn.swg $(DSTROOT)$(INSTALL_PREFIX)/share/swig/$(ProjectVersion)
	$(CP) $(DSTROOT)$(INSTALL_PREFIX)/bin/swig $(SYMROOT)
	$(STRIP) -x $(DSTROOT)$(INSTALL_PREFIX)/bin/swig
	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENSE $(OSL)/$(Project).txt
	$(MKDIR) $(DSTROOT)$(INSTALL_PREFIX)/share/man/man1
	$(INSTALL_FILE) $(SRCROOT)/swig.1 $(DSTROOT)$(INSTALL_PREFIX)/share/man/man1/swig.1
