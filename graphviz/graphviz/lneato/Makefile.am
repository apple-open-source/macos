## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

man_MANS = lneato.1
bin_SCRIPTS = lneato

lneato: ${top_srcdir}/lneato/lneato.sh
	cp ${top_srcdir}/lneato/lneato.sh lneato
	chmod +x lneato

EXTRA_DIST = lneato.1 lneato.bsh lneato.ksh lneato.sh \
	Makefile.old mswin32

DISTCLEANFILES = lneato
