## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

info_TEXINFOS = dejagnu.texi

TARGETS = overview.rtf overview.html overview.dvi overview.ps

docs: $(TARGETS)

%.ps: %.dvi
	dvips -o $@ $<

%.pdf: %.sgml
	db2pdf $<

%.dvi: %.sgml
	db2dvi $<

%.rtf: %.sgml
	db2rtf -o $@ $<

%.gif: %.fig
	convert -transparency white $< $@ # .fig -> .gif

%.epsi: %.eps
	ps2epsi $<                                     # .eps -> .epsi

%.eps: %.fig
	fig2dev -L ps -m 0.7 -p dummy $< > $@          # .fig -> .eps/portrait

%.html: %.sgml
	db2html $<

# now for some extra dependencies that the automatic rules will not
# catch:

overview.pdf overview.ps overview.dvi overview.rtf overview.html: overview.sgml ref.sgml user.sgml

clean:
	rm -f $(TARGETS)

# install-data-local: overview.pdf overview.html
#	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/html
#	$(INSTALL_DATA) overview/*.html $(DESTDIR)$(pkgdatadir)/html
#	$(INSTALL_DATA) overview.ps $(DESTDIR)$(pkgdatadir)/dejagnu.ps
#	$(INSTALL_DATA) overview.pdf $(DESTDIR)$(pkgdatadir)/dejagnu.pdf
