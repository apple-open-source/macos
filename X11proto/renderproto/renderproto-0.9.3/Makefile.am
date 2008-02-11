renderdir = $(includedir)/X11/extensions
render_HEADERS = \
	render.h \
	renderproto.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = renderproto.pc

renderdocdir = $(datadir)/doc/$(PACKAGE)
renderdoc_DATA = renderproto.txt

EXTRA_DIST = autogen.sh renderproto.pc.in $(renderdoc_DATA)
