## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

AM_CPPFLAGS = \
	-I$(top_srcdir)/lefty/ws/x11 \
	-I$(top_srcdir)/lefty/os/unix \
	-I$(top_srcdir)/lefty/dot2l \
	@XAW_INCLUDES@

LIBS =

SUBDIRS = ws os cs2l dot2l examples

leftydir = $(datadir)/@PACKAGE@/lefty

AM_CFLAGS = -DLEFTYPATH=\"${leftydir}\" -DHAVEDOT @X_CFLAGS@

noinst_HEADERS = code.h common.h display.h exec.h g.h gfxview.h internal.h \
	io.h lex.h mem.h parse.h str.h tbl.h txtview.h
if WITH_X
bin_PROGRAMS = lefty
man_MANS = lefty.1
endif

lefty_SCRIPTS = lefty.psp

lefty_SOURCES = code.c common.c display.c exec.c g.c gfxview.c internal.c \
	lefty.c lex.c mem.c parse.c str.c tbl.c txtview.c

lefty_LDADD = $(top_builddir)/lefty/dot2l/libdot2l.la \
	$(top_builddir)/lefty/ws/x11/libws.la \
	$(top_builddir)/lefty/ws/x11/libfilereq/libfilereq.la \
	$(top_builddir)/lefty/os/unix/libos.la \
	@X_LIBS@ @XAW_LIBS@ -lXt @X_PRE_LIBS@ \
	-lXaw -lXmu -lXext -lX11 @XPM_LIBS@ \
	@SOCKET_LIBS@ @LIBS@

EXTRA_DIST = $(lefty_SCRIPTS) $(lefty_SOURCES) lefty.1 colors.txt \
	Makefile.old aix_mods

