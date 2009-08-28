Project        = pcre
ProjectVersion = 7.8
Patches        = Makefile.in.diff

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

PREFIX=/usr/local
LIBDIR=/usr/lib
ManPageDirectories=/usr/local/share/man

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@for file in $(Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$file) || exit 1; \
	done

install::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure --disable-dependency-tracking \
		--prefix=$(PREFIX) --libdir=$(LIBDIR) --disable-static \
		--enable-unicode-properties \
		--disable-cpp
	$(MAKE) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)

	for bin in pcregrep pcretest; do \
		$(CP) $(DSTROOT)$(PREFIX)/bin/$${bin} $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(PREFIX)/bin/$${bin}; \
	done

	for lib in .0.0.1 posix.0.0.0; do \
		$(CP) $(DSTROOT)$(LIBDIR)/libpcre$${lib}.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(LIBDIR)/libpcre$${lib}.dylib; \
	done

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENCE $(OSL)/$(Project).txt

	@$(MAKE) compress_man_pages
