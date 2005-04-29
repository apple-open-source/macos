## Global rules and macros to be included in all Makefiles.


# Variables

export STP_MODULE_PATH = $(top_builddir)/src/main/.libs:$(top_builddir)/src/main
export STP_DATA_PATH = $(top_srcdir)/src/main

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include $(LOCAL_CPPFLAGS) $(GNUCFLAGS)

LIBS = $(INTLLIBS) @LIBS@

# Libraries

GIMPPRINT_LIBS = $(top_builddir)/src/main/libgimpprint.la
GIMPPRINTUI_LIBS = $(top_builddir)/src/libgimpprintui/libgimpprintui.la
GIMPPRINTUI2_LIBS = $(top_builddir)/src/libgimpprintui2/libgimpprintui2.la

# Rules

$(top_builddir)/src/main/libgimpprint.la:
	cd $(top_builddir)/src/main; \
	$(MAKE)

$(top_builddir)/src/libgimpprintui/libgimpprintui.la:
	cd $(top_builddir)/src/libgimpprintui; \
	$(MAKE)

$(top_builddir)/src/libgimpprintui2/libgimpprintui2.la:
	cd $(top_builddir)/src/libgimpprintui2; \
	$(MAKE)
