
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-atom.la

xcbinclude_HEADERS = xcb_atom.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_atom_la_SOURCES = atoms.c atomlist.m4 atoms.gperf.m4 xcb_atom.h.m4
libxcb_atom_la_CPPFLAGS = $(XCB_CFLAGS)
libxcb_atom_la_LIBADD = $(XCB_LIBS)
libxcb_atom_la_LDFLAGS = -version-info 1:0:0

pkgconfig_DATA = xcb-atom.pc

EXTRA_DIST = xcb-atom.pc.in

BUILT_SOURCES = atoms.c atoms.gperf xcb_atom.h
CLEANFILES = $(BUILT_SOURCES)

atoms.c: atoms.gperf
	$(GPERF) --output-file $@ $<

atoms.gperf: atoms.gperf.m4 atomlist.m4
	$(M4) -I$(srcdir) $< >$@

xcb_atom.h: xcb_atom.h.m4 atomlist.m4
	$(M4) -I$(srcdir) $< >$@
