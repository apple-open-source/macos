##
# Makefile for gimp-print
##

# Project info
Project               = gimp-print
UserType              = Administrator
ToolType              = Services

Extra_Configure_Flags = --sysconfdir=/private/etc		\
						--infodir=/usr/local/share/info \
						--mandir=/usr/local/share/man	\
						--with-cups						\
						--with-user-guide				\
						--with-samples					\
						--with-escputil					\
						--with-included-gettext			\
						--without-translated-ppds		\
						--disable-static

#						--with-foomatic					

Extra_Install_Flags   = sysconfdir="$(DSTROOT)$(ETCDIR)"
Extra_CC_Flags        = -fno-common -I ../../intl
GnuAfterInstall       = do-fixups

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

do-fixups:
	strip -S	"$(DSTROOT)/usr/bin/escputil" \
			"$(DSTROOT)/usr/lib/libgimpprint.1.1.0.dylib" \
			"$(DSTROOT)/usr/bin/cups-calibrate" \
			"$(DSTROOT)/usr/libexec/cups/backend/canon" \
			"$(DSTROOT)/usr/libexec/cups/backend/epson" \
			"$(DSTROOT)/usr/libexec/cups/filter/commandtocanon" \
			"$(DSTROOT)/usr/libexec/cups/filter/commandtoepson" \
			"$(DSTROOT)/usr/libexec/cups/filter/rastertoprinter"
	rm -f		"$(DSTROOT)/usr/lib/charset.alias" \
			"$(DSTROOT)/usr/lib/libgimpprint.la" \
			"$(DSTROOT)/usr/share/info/dir" \
			"$(DSTROOT)/usr/share/locale/locale.alias"
	mv		"$(DSTROOT)/usr/share/locale/en_GB" \
			"$(DSTROOT)/usr/share/locale/en_GB.ISO8859-1"
	zcat "$(DSTROOT)/usr/share/cups/model/C/pcl-900.ppd.gz" | \
		sed 's/HP DeskJet 900 series/HP DeskJet 995C/g' | \
		gzip >	"$(DSTROOT)/usr/share/cups/model/C/pcl-995C.ppd.gz" 
	zcat "$(DSTROOT)/usr/share/cups/model/C/pcl-340.ppd.gz" | \
		sed 's/HP DeskJet 340/HP dj450/g' | \
		gzip >	"$(DSTROOT)/usr/share/cups/model/C/pcl-450.ppd.gz" 
