man_MANS = nano.1 nanorc.5 rnano.1

mandir = @mandir@/fr

BUILT_SOURCES = nano.1.html nanorc.5.html rnano.1.html

nano.1.html: nano.1
	cat $< | groff -t -mandoc -Thtml > $@
nanorc.5.html: nanorc.5
	cat $< | groff -t -mandoc -Thtml > $@
rnano.1.html: rnano.1
	cat $< | groff -t -mandoc -Thtml > $@

EXTRA_DIST = $(man_MANS) $(BUILT_SOURCES)
