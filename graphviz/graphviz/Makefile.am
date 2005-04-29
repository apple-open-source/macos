## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4 foreign

pkgindexdir = $(libdir)/@PACKAGE@
pkgindex_DATA = pkgIndex.tcl

SUBDIRS = cdt agraph agutil doc pathplan graph gd dotneato \
	dotty graphs lefty lneato ns \
	tclstubs tclhandle gdtclft tcldot tclpathplan \
        tools contrib dynagraph windows

#dag fdp geo grid incr shape tcldgr tcldgl

pkgIndex.tcl: @TCL_PKGINDEX@ @TK_PKGINDEX@
	echo "# end" >end
	cat @TCL_PKGINDEX@ @TK_PKGINDEX@ end >$@
	rm -f end

doxygen:
	doxygen

dist-hook:
	find $(distdir) -name CVS -type d -prune -exec rm -rf {} \;

EXTRA_DIST = graphviz.spec.in graphviz.spec graphviz.cvt \
	FAQ.txt MINTERMS.txt autogen.sh depcomp ast_common.h.in \
	Makefile.old Config.mk configure.old INSTALL.old \
	Makeargs config.h.old compat_getopt.h LICENSE.html iffe \
	makearch dot.demo features m4

DISTCLEANFILES = pkgIndex.tcl ast_common.h FEATURE/* graphviz-@VERSION@*
