printdir = $(includedir)/X11/extensions
print_HEADERS = \
	Print.h \
	Printstr.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = printproto.pc

miscmandir = $(MISC_MAN_DIR)
miscman_DATA =	man/Xprint.$(MISC_MAN_SUFFIX)
CLEANFILES = $(miscman_DATA)

EXTRA_DIST = 	printproto.pc.in	\
		man/Xprint.man		\
		man/Xprint.sgml		\
		specs/xp_proto.book	\
		specs/xp_proto.mif	\
		specs/xp_proto.ps	\
		specs/xp_protoIX.doc	\
		specs/xp_protoTOC.doc	\
		specs/xp_proto_cov.mif

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = \
        -e 's|__vendorversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
        -e 's|__xorgversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
        -e 's|__datadir__|$(datadir)|g' \
        -e 's|__libdir__|$(libdir)|g' \
        -e 's|__sysconfdir__|$(sysconfdir)|g' \
        -e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
        -e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
        -e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g'

SUFFIXES = .$(MISC_MAN_SUFFIX) .man

.man.$(MISC_MAN_SUFFIX): man
	$(MKDIR_P) man
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
