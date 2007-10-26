##
# Makefile for gutenprint
##

# Project info
Project               = gutenprint
UserType              = Administrator
ToolType              = Services
LD_TWOLEVEL_NAMESPACE =
LIBTOOL_CMD_SEP       = ^

Extra_Configure_Flags	= --prefix=/usr			\
			  --sysconfdir=/private/etc	\
			  --disable-static		\
			  --disable-libgutenprintui	\
			  --disable-libgutenprintui2	\
			  --disable-samples		\
			  --disable-test		\
			  --disable-testpattern		\
			  --enable-cups-ppds		\
			  --enable-cups-level3-ppds	\
			  --without-modules

GnuAfterInstall       = do-fixups install-plist

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Flags = DESTDIR=$(DSTROOT)

do-fixups:
	echo Stripping symbols from binaries...
	find $(DSTROOT) -type f -perm +111 -print0 | xargs -0tn 1 strip -S
	echo Remove the 64-bit slices from everything but the libraries...
	@for i in `find $(DSTROOT) -type f -perm +111 ! -name "lib*.dylib"`; do \
		if $(LIPO) -info $$i | $(GREP) -qe ppc64 -e x86_64; then \
			echo "Removing 64-bit architectures from $$i"; \
			$(LIPO) -remove ppc64 -remove x86_64 $$i -output $$i.thin && $(MV) $$i.thin $$i; \
		fi \
	done
	echo Removing unwanted files...
	rm -fr	"$(DSTROOT)/usr/include" \
		"$(DSTROOT)/usr/lib/gimp" \
		"$(DSTROOT)/usr/lib/gutenprint" \
		"$(DSTROOT)/usr/lib/libgutenprint.la" \
		"$(DSTROOT)/usr/lib/pkgconfig" \
		"$(DSTROOT)/usr/share/gutenprint/doc/html" \
		"$(DSTROOT)/usr/share/gutenprint/doc/gutenprint-users-manual.odt" \
		"$(DSTROOT)/usr/share/gutenprint/doc/reference-html" \
		"$(DSTROOT)/usr/share/gutenprint/samples"
	echo Making the man page file names match their binaries...
	mv	"$(DSTROOT)/usr/share/man//man8/cups-genppd.8" \
		"$(DSTROOT)/usr/share/man//man8/cups-genppd.5.1.8"
	mv	"$(DSTROOT)/usr/share/man//man8/cups-genppdconfig.8" \
		"$(DSTROOT)/usr/share/man//man8/cups-genppdconfig.5.1.8"
	mv	"$(DSTROOT)/usr/share/man//man8/cups-genppdupdate.8" \
		"$(DSTROOT)/usr/share/man//man8/cups-genppdupdate.5.1.8"

OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt
