##
# Makefile for gimp-print
##

# Project info
Project               = gimp-print
UserType              = Administrator
ToolType              = Services
LD_TWOLEVEL_NAMESPACE =
LIBTOOL_CMD_SEP       = ^

Extra_Configure_Flags = --sysconfdir=/private/etc		\
			--infodir=/usr/local/share/info \
			--mandir=/usr/local/share/man \
			--with-cups \
			--enable-cups-ppds \
			--enable-translated-cups-ppds \
			--disable-testpattern

Extra_CC_Flags        = -fno-common -I ../../intl
Extra_Install_Flags   = LIBTOOL_CMD_SEP=^
GnuAfterInstall       = do-fixups

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

do-fixups:
	find $(DSTROOT) -type f -perm +111 -print0 | xargs -0tn 1 strip -S
	rm -fr	"$(DSTROOT)/usr/include" \
		"$(DSTROOT)/usr/lib/charset.alias" \
		"$(DSTROOT)/usr/lib/gimp" \
		"$(DSTROOT)/usr/lib/libgimpprint.a" \
		"$(DSTROOT)/usr/lib/libgimpprint.la" \
		"$(DSTROOT)/usr/lib/pkgconfig/gimpprintui.pc" \
		"$(DSTROOT)/usr/lib/pkgconfig/gimpprintui2.pc" \
		"$(DSTROOT)/usr/sbin" \
		"$(DSTROOT)/usr/share/gimp-print/doc/html" \
		"$(DSTROOT)/usr/share/gimp-print/doc/reference-html" \
		"$(DSTROOT)/usr/share/gimp-print/samples" \
		"$(DSTROOT)/usr/share/info/dir" \
		"$(DSTROOT)/usr/share/locale/locale.alias" \
		"$(DSTROOT)/usr/share/man/man8/cups-genppd.8" \
		"$(DSTROOT)/usr/share/man/man8/cups-genppdconfig.8" \
		"$(DSTROOT)/usr/share/man/man8/cups-genppdupdate.8"
