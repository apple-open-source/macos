## Process this file with automake to generate Makefile.in

# AUTOMAKE_OPTIONS =

man_MANS = runtest.1
# FIXME: We need to generate an info doc from SGML somehow, but for
# now, don't install the old texinfo document from version 1.3
# info_TEXINFOS = dejagnu.texi
DOCBOOK = @DOCBOOK@
TARGETS = overview.html overview.ps overview.pdf overview.rtf

docs: $(TARGETS)

%.ps: %.dvi
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	dvips -o $@ $*.dvi

%.pdf: %.sgml
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	$(DOCBOOK)/db2pdf $${dir}/$*.sgml

%.dvi: %.sgml
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	$(DOCBOOK)/db2dvi $${dir}/$*.sgml

%.rtf: %.sgml
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	$(DOCBOOK)/db2rtf $${dir}/$*.sgml

%.gif: %.fig
	convert -transparency white $< $@ # .fig -> .gif

%.epsi: %.eps
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	ps2epsi $${dir}/$*.sgml                        # .eps -> .epsi

%.eps: %.fig
	fig2dev -L ps -m 0.7 -p dummy $< > $@          # .fig -> .eps/portrait

%.html: %.sgml
	src=$(srcdir); \
	dir=`(cd $${src}; pwd)`; \
	mkdir html; \
	$(DOCBOOK)/db2html -o html $${dir}/$*.sgml

# now for some extra dependencies that the automatic rules will not
# catch:

html/book1.html rtf overview.pdf overview.ps overview.dvi overview.rtf overview.html: overview.sgml ref.sgml user.sgml 

html/book1.html: overview.sgml ref.sgml user.sgml 

clean realclean distclean:
	rm -fr $(TARGETS) DBHTOHTML* html dejagnu.info* overview.junk overview overview.{aux,dvi,log,ps,pdf,tex}

install-doc: $(TARGETS)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/html $(DESTDIR)$(pkgdatadir)/html/stylesheet-images
	$(INSTALL_DATA)	html/*.html $(DESTDIR)$(pkgdatadir)/html
	$(INSTALL_DATA) overview.rtf $(DESTDIR)$(pkgdatadir)/dejagnu.rtf
	$(INSTALL_DATA) overview.ps $(DESTDIR)$(pkgdatadir)/dejagnu.ps
	$(INSTALL_DATA) overview.pdf $(DESTDIR)$(pkgdatadir)/dejagnu.pdf
	$(INSTALL_DATA) $(srcdir)/runtest.1 $(mandir)/man1/


