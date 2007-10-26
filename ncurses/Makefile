# Project info
Project           = ncurses
ProjectVersion    = 5.5
Patches           = make.diff hex.diff dynamic-no-pic.diff wgetbkgrnd.diff

# This shouldn't change, needed for compatibility.
ABIVersion        = 5.4

Configure_Flags = --with-shared --without-normal --without-debug \
	--enable-termcap --enable-widec --with-abi-version=$(ABIVersion) \
	--without-cxx-binding --without-cxx \
	--mandir=$(MANDIR)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

# Extract the source.
install_source::
	$(RMDIR) $(SRCROOT)/$(Project) $(SRCROOT)/$(Project)-$(ProjectVersion)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(Project)-$(ProjectVersion).tar.gz
	$(MV) $(SRCROOT)/$(Project)-$(ProjectVersion) $(SRCROOT)/$(Project)
	@for patchfile in $(Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done

install::
	cd $(OBJROOT) && $(Environment) $(SRCROOT)/$(Project)/configure \
		--prefix=/usr --disable-dependency-tracking \
		$(Configure_Flags)
	$(MAKE) -C $(OBJROOT)
	$(MAKE) -C $(OBJROOT) install DESTDIR=$(DSTROOT)

	$(MKDIR) $(OSV) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(TOUCH) $(OSL)/$(Project).txt
	head -n 28 < $(SRCROOT)/$(Project)/Makefile.in | sed 1d > $(OSL)/$(Project).txt

	$(RM) $(DSTROOT)/usr/lib/libtermcap.dylib
	$(LN) -s libncurses.$(ABIVersion).dylib $(DSTROOT)/usr/lib/libtermcap.dylib
	tar -C $(DSTROOT) -xjf $(SRCROOT)/libncurses.5.dylib.tar.bz2

	$(MKDIR) $(SYMROOT)/usr/lib
	@for library in form menu ncurses panel; do \
		$(CP) $(DSTROOT)/usr/lib/lib$${library}.$(ABIVersion).dylib $(SYMROOT)/usr/lib; \
		$(STRIP) -x $(DSTROOT)/usr/lib/lib$${library}.$(ABIVersion).dylib; \
	done
	$(MKDIR) $(SYMROOT)/usr/bin
	@for binary in clear infocmp tack tic toe tput tset; do \
		lipo -remove x86_64 -remove ppc64 -output $(DSTROOT)/usr/bin/$${binary} $(DSTROOT)/usr/bin/$${binary}; \
		$(CP) $(DSTROOT)/usr/bin/$${binary} $(SYMROOT)/usr/bin; \
		$(STRIP) -x $(DSTROOT)/usr/bin/$${binary}; \
	done

	$(MAKE) compress_man_pages
	echo ".so man3/curs_termcap.3x.gz" > $(DSTROOT)/usr/share/man/man3/termcap.3
