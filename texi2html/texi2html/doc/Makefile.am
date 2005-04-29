#
info_TEXINFOS = texi2html.texi

CLEANFILES = texi2html.ps texi2html.pdf texi2html.html \
             texi2html.ascii
#
texinfodata = $(datadir)/texinfo
texinfohtmldir = $(texinfodata)/html
texinfohtml_DATA    = texi2html.html

# use the script out of the distribution NOT
# an old one which is installed on the client.
TEXI2HTML = $(top_builddir)/texi2html

MAINTAINERCLEANFILES = \
	Makefile.in \
	texi2thml.html

all: info texi2html.html

texi2html.html: texi2html.texi $(srcdir)/version.texi $(TEXI2HTML) Makefile
	TEXINPUTS=$(srcdir)::$$TEXINPUTS \
	  $(PERL) $(TEXI2HTML) -V $<

texi2html.pdf: texi2html.texi $(srcdir)/version.texi $(texi2html_TEXINFOS)
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf $<

dvi: texi2html.dvi

ps: texi2html.ps

pdf: texi2html.pdf

html: texi2html.html

txt: texi2html.txt

SUFFIXES = .txt

# texinfo based targets automake neglects to include
.texinfo.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
.txi.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
.texi.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
