lib_LTLIBRARIES = libXp.la

libXp_la_SOURCES =			\
	XpAttr.c			\
	XpContext.c			\
	XpDoc.c				\
	XpExtUtil.c			\
	XpExtVer.c			\
	XpGetData.c			\
	XpImageRes.c			\
	XpInput.c			\
	XpJob.c				\
	XpLocale.c			\
	XpNotifyPdm.c			\
	XpPage.c			\
	XpPageDim.c			\
	XpPrinter.c			\
	XpPutData.c			\
	XpScreens.c			\
					\
	XpExtUtil.h

libXp_la_LIBADD = $(XPRINT_LIBS)

AM_CFLAGS = $(CWARNFLAGS) $(XPRINT_CFLAGS) $(MALLOC_ZERO_CFLAGS)

INCLUDES = -I$(top_srcdir)/include/X11/extensions

#
# Library version number. This must match old versions on
# every platform, so until we change the ABI in an incompatible fashion,
# we have to use -version-number
#
XP_VERSION=-version-number 6:2:0
libXp_la_LDFLAGS = $(XP_VERSION) -no-undefined
