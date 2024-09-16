Project        = pcre
ProjectVersion = 8.44
Patches        = Makefile.in.diff no-programs.diff configure.diff

Project2        = pcre2
ProjectVersion2 = 10.42
Patches2        = ''

JOBS_NUM = $(shell sysctl -n hw.logicalcpu)
JOBS = -j$(JOBS_NUM)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

override MACOSX_DEPLOYMENT_TARGET := 13.0
export MACOSX_DEPLOYMENT_TARGET

PREFIX=/usr/local
LIBDIR=/usr/lib
ManPageDirectories=/usr/local/share/man

CFLAGS += -fno-typed-cxx-new-delete -fno-typed-memory-operations
CCC_OVERRIDE_OPTIONS+=+-fno-typed-cxx-new-delete +-fno-typed-memory-operations
export CC_OVERRIDE_OPTIONS
export CFLAGS

install::
# Extract the source.
# PCRE
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.bz2
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@for file in $(Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/files/$$file) || exit 1; \
	done

# PCRE2
	$(RMDIR) $(SRCROOT)/$(Project2) $(SRCROOT)/$(Project2)-$(ProjectVersion2)
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(Project2)-$(ProjectVersion2).tar.bz2
	$(MV) $(SRCROOT)/$(Project2)-$(ProjectVersion2) $(SRCROOT)/$(Project2)

# Build the source
# PCRE
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure --disable-dependency-tracking \
		--prefix=$(PREFIX) --libdir=$(LIBDIR) --disable-static \
		--enable-unicode-properties \
		--disable-cpp
	$(MAKE) $(JOBS) -C $(OBJROOT) EXTRA_LIBPCRE_LDFLAGS="-version-info 0:1:0" EXTRA_LIBPCREPOSIX_LDFLAGS="-version-info 0:0:0" EXTRA_LIBPCRECPP_LDFLAGS="-version-info 0:0:0"
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)

	for lib in .0 posix.0; do \
		$(CP) $(DSTROOT)$(LIBDIR)/libpcre$${lib}.dylib $(SYMROOT); \
		$(STRIP) -x $(DSTROOT)$(LIBDIR)/libpcre$${lib}.dylib; \
	done

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL_FILE) $(SRCROOT)/$(Project)/LICENCE $(OSL)/$(Project).txt

	@$(MAKE) compress_man_pages

# PCRE2
	$(RMDIR) -rf $(OBJROOT)/*
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project2)/configure --disable-dependency-tracking \
		--prefix=$(PREFIX) --libdir=$(LIBDIR)
	$(MAKE) $(JOBS) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)
# move the static libraries to their own folder
	mkdir -p $(DSTROOT)/usr/local/lib/pcre2-static
	mv $(DSTROOT)/usr/lib/*.a $(DSTROOT)/usr/local/lib/pcre2-static

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project2)/LICENCE $(OSL)/$(Project2).txt

	@$(MAKE) compress_man_pages
