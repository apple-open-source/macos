lib_LTLIBRARIES = liblbxutil.la

noinst_PROGRAMS = mkg3states

mkg3states_SOURCES =				\
	$(srcdir)/image/mkg3states.c

liblbxutil_la_SOURCES =				\
	$(srcdir)/lbx_zlib/reqstats.h		\
	$(srcdir)/lbx_zlib/lbx_zlib_io.c	\
	$(srcdir)/lbx_zlib/lbx_zlib.h		\
	$(srcdir)/lbx_zlib/lbx_zlib.c		\
	$(srcdir)/lbx_zlib/reqstats.c		\
	$(srcdir)/image/misc.c			\
	$(srcdir)/image/dpackbits.c		\
	$(srcdir)/image/epackbits.c		\
	$(srcdir)/image/dfaxg42d.c		\
	$(srcdir)/image/lbxfax.h		\
	$(srcdir)/image/efaxg42d.c		\
	$(srcdir)/image/lbxbwcodes.h		\
	$(srcdir)/delta/lbxdelta.c

nodist_liblbxutil_la_SOURCES = g3states.h

liblbxutil_la_LIBADD = @LBXUTIL_LIBS@

INCLUDES = @LBXUTIL_CFLAGS@ 

$(srcdir)/image/dfaxg42d.c: g3states.h

g3states.h: mkg3states
	-rm -f g3states.h
	./mkg3states -c > g3states.h_ && mv g3states.h_ g3states.h

liblbxutil_la_LDFLAGS = -version-number 1:0:0 -no-undefined

CLEANFILES = g3states.h
