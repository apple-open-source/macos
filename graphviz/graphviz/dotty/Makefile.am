## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

DOTTYSCRIPTS = dotty.lefty dotty_draw.lefty dotty_edit.lefty \
	dotty_layout.lefty dotty_ui.lefty

leftydir = $(datadir)/@PACKAGE@/lefty
lefty_SCRIPTS = $(DOTTYSCRIPTS)

man_MANS = dotty.1
bin_SCRIPTS = dotty

dotty: ${top_srcdir}/dotty/dotty.sh
	cp ${top_srcdir}/dotty/dotty.sh dotty
	chmod +x dotty

EXTRA_DIST = $(lefty_SCRIPTS) dotty.1 dotty.bsh dotty.ksh dotty.sh \
	Makefile.old notes mswin32

DISTCLEANFILES = dotty
