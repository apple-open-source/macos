if BUILD_SXPM

bin_PROGRAMS = sxpm

sxpm_CFLAGS = $(SXPM_CFLAGS)

INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include

sxpm_SOURCES = sxpm.c

sxpm_LDADD = $(XPM_LIBS) $(SXPM_LIBS) $(top_builddir)/src/libXpm.la

# Man page
appmandir = $(APP_MAN_DIR)

appman_PRE = sxpm.man
appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

CLEANFILES = $(appman_DATA)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = \
	-e 's|__vendorversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xorgversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xservername__|Xorg|g' \
	-e 's|__xconfigfile__|xorg.conf|g' \
	-e 's|__projectroot__|$(prefix)|g' \
	-e 's|__apploaddir__|$(appdefaultdir)|' \
	-e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
	-e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
	-e 's|__adminmansuffix__|$(ADMIN_MAN_SUFFIX)|g' \
	-e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g' \
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g' 

SUFFIXES = .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@

endif

EXTRA_DIST = \
	plaid_ext.xpm \
	plaid_mask.xpm \
	plaid.xpm \
	sxpm.man
