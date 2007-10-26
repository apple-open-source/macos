lib_LTLIBRARIES =

#
# This doesn't appear to be used on any
# current systems -- it requires SUNSHLIB and !SHAREDCODE,
# but only sunLib.rules defines SUNSHLIB and that file also
# always defines SHAREDCODE. Go figure
#
# SHAREDLIB_SOURCES = sharedlib.c

COMMON_SOURCES = \
	Actions.c \
	AllWidgets.c \
	AsciiSink.c \
	AsciiSrc.c \
	AsciiText.c \
	Box.c \
	Command.c \
	Converters.c \
	Dialog.c \
	DisplayList.c \
	Form.c \
	Grip.c \
	Label.c \
	List.c \
	MenuButton.c \
	MultiSrc.c \
	MultiSink.c \
	OS.c \
	Paned.c \
	Panner.c \
	Pixmap.c \
	Porthole.c \
	Private.h \
	Repeater.c \
	Scrollbar.c \
	Simple.c \
	SimpleMenu.c \
	Sme.c \
	SmeBSB.c \
	SmeLine.c \
	StripChart.c \
	Text.c \
	TextSink.c \
	TextSrc.c \
	TextAction.c \
	TextPop.c \
	TextTr.c \
	Toggle.c \
	Tree.c \
	Vendor.c \
	Viewport.c \
	XawIm.c \
	XawInit.c \
	XawI18n.c \
	XawI18n.h

COMMON_CFLAGS = \
	-I${top_srcdir}/include \
	-I${top_srcdir}/include/X11/Xaw \
	-DPROJECT_ROOT=\"$(prefix)\"

if BUILD_XAW6

lib_LTLIBRARIES += libXaw6.la

libXaw6_la_CFLAGS =			\
	$(COMMON_CFLAGS)		\
	-DOLDXAW			\
	$(XAW6_CFLAGS)

libXaw6_la_SOURCES = \
	$(COMMON_SOURCES)

libXaw6_la_LDFLAGS = -version-info 6:1:0 -no-undefined
libXaw6_la_LIBADD = $(XAW6_LIBS)

if !PLATFORM_WIN32
install-exec-hook::
	-rm -f $(DESTDIR)$(libdir)/libXaw.so.6
	(cd $(DESTDIR)$(libdir) && ln -s libXaw6.so.6 libXaw.so.6)
endif

endif

if BUILD_XAW7

lib_LTLIBRARIES += libXaw7.la

libXaw7_la_CFLAGS =			\
	$(COMMON_CFLAGS)		\
	-DXAW7				\
	$(XAW7_CFLAGS)

libXaw7_la_SOURCES = \
	$(COMMON_SOURCES) \
	Tip.c

libXaw7_la_LDFLAGS = -version-info 7:0:0 -no-undefined
libXaw7_la_LIBADD = $(XAW7_LIBS)

if !PLATFORM_WIN32
install-exec-hook::
	-rm -f $(DESTDIR)$(libdir)/libXaw.so.7
	(cd $(DESTDIR)$(libdir) && ln -s libXaw7.so.7 libXaw.so.7)
endif

endif

if BUILD_XAW8

lib_LTLIBRARIES += libXaw8.la

libXaw8_la_CFLAGS =			\
	$(COMMON_CFLAGS)		\
	$(XAW8_CFLAGS)

libXaw8_la_SOURCES = \
	$(COMMON_SOURCES) \
	PrintShell.c \
	Tip.c

libXaw8_la_LDFLAGS = -version-info 8:0:0 -no-undefined
libXaw8_la_LIBADD = $(XAW8_LIBS)

if !PLATFORM_WIN32
install-exec-hook::
	-rm -f $(DESTDIR)$(libdir)/libXaw.so.8
	(cd $(DESTDIR)$(libdir) && ln -s libXaw8.so.8 libXaw.so.8)
endif

endif

EXTRA_DIST = sharedlib.c
