# file: .../doc/makefile
#
# - build the user documentation, snacc-*.ps
# - install the various manual pages
#
#  MS 92
#  use nroff -man snacc.1  to see what the man page, snacc.1, looks like
#
#  if latex bombs unexplainably, try 'make clean' followed by 'make'
#
# $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/doc/makefile,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:05:53  aram
# Originals from SMIME Free Library.
#
# Revision 1.4  1997/06/19 09:28:30  wan
# Disabled making new doc due to new version string.
#
# Revision 1.3  1997/02/16 19:23:13  rj
# typo fixed
#
# Revision 1.2  1997/01/02 08:51:51  rj
# order of two files reversed to reflect the order in snacc.tex
#
# Revision 1.1  1997/01/01 22:28:25  rj
# first check-in
#

include ../makehead

TOP		= ..

MANUALS.1 = \
	snacc.1		\
	mkchdr.1	\
	ptbl.1		\
	pval.1		\
	snacced.1

MANUALS.n = \
	snacc.n

TEX_ROOT	= snacc.tex

TEX_SRC		= \
	$(TEX_ROOT)	\
	misc-hyph.tex	\
	version.tex	\
	intro-1.2.tex	\
	intro-1.1.tex	\
	design.tex	\
	c-gen.tex	\
	c-lib.tex	\
	c++-gen.tex	\
	c++-lib.tex	\
	meta.tex	\
	tcl.tex		\
	editor.tex	\
	idl-gen.tex	\
	ttab.tex	\
	modifying.tex	\
	future-work.tex	\
	asn1-defs.tex	\
	coding.tex	\
	makefile.tex

AUX		= \
	snacc.aux	\
	version.aux	\
	intro-1.1.aux	\
	intro-1.2.aux	\
	design.aux	\
	c-gen.aux	\
	c-lib.aux	\
	c++-gen.aux	\
	c++-lib.aux	\
	meta.aux	\
	tcl.aux		\
	editor.aux	\
	idl-gen.aux	\
	ttab.aux	\
	modifying.aux	\
	future-work.aux	\
	asn1-defs.aux	\
	coding.aux	\
	makefile.aux

side		= \
	$(AUX)	\
	snacc.toc

side.cmp	= \
	snacc.aux~	\
	version.aux~	\
	intro-1.1.aux~	\
	intro-1.2.aux~	\
	design.aux~	\
	c-gen.aux~	\
	c-lib.aux~	\
	c++-gen.aux~	\
	c++-lib.aux~	\
	meta.aux~	\
	tcl.aux~	\
	editor.aux~	\
	idl-gen.aux~	\
	ttab.aux~	\
	modifying.aux~	\
	future-work.aux~\
	asn1-defs.aux~	\
	coding.aux~	\
	makefile.aux~	\
	snacc.toc~

BIB_SRC		= snacc.bib corba.bib tcl.bib
BIB_BBL		= snacc.bbl

EPS	= \
	example.eps	\
	recur.eps	\
	selbox.eps	\
	simple.eps	\
	struct.eps	\
	str-popup.eps	\
	list-popup.eps	\

DVI		= snacc.dvi
A4.PS		= snacc-a4.ps
A5.PS		= snacc-a5.ps
A6.PS		= snacc-a6.ps
BOOK.PS		= snacc-book.ps

DISTFILES	= \
	makefile	\
	tumble.sed	\
	$(MANUALS.1)	\
	$(MANUALS.n)	\
	$(TEX_SRC)	\
	$(BIB_SRC)	\
	$(A5.PS)

#-------------------------------------------------------------------------------

default::	a4
#default::	$(A5.PS) $(BOOK.PS)
all::		a4 a5 a6 book

book::		$(BOOK.PS)
$(BOOK.PS):	$(A4.PS) tumble.sed
		$(REASON)
		< $< $(PSBOOK) | $(PSNUP) -2 | sed -f tumble.sed > $@~
		mv $@~ $@

a5::		$(A5.PS)
$(A5.PS):	$(A4.PS)
		$(REASON)
		< $< $(PSNUP) -2 > $@~
		mv $@~ $@

a6::		$(A6.PS)
$(A6.PS):	$(A4.PS)
		$(REASON)
		< $< $(PSNUP) -4 > $@~
		mv $@~ $@

a4::		$(A4.PS)
$(A4.PS):	$(DVI)
		$(REASON)
#		$(DVIPS) -f $< > $@~
#		mv $@~ $@
# dvips doesn't return a useful return code:
		$(DVIPS) -f $< > $@
#		$(PSPREVIEW) $@

dvi::		$(DVI)
$(DVI):		$(TEX_SRC) $(BIB_BBL) $(side.cmp) $(EPS)
		$(REASON)
		trap "$(RM) $@ `basename $(TEX_ROOT) .tex`.aux" 2; $(LATEX) $(TEX_ROOT) || { e=$$?; $(RM) $@ `basename $(TEX_ROOT) .tex`.aux; exit $$e; }
		for f in $(side); do\
		  if cmp $$f $$f~; then\
		    :;\
		  else\
		    cp $$f $$f~;\
		  fi;\
		done
		$(MAKE) $(BIB_BBL) $@

$(BIB_BBL):	$(BIB_SRC) $(side.cmp)
		$(REASON)
		if [ -s snacc.aux ]; then $(BIBTEX) `basename $(TEX_ROOT) .tex`; fi

$(side.cmp) redo:
		touch $@

version.tex:	#$(TOP)/version.h
		echo '\def\snaccversion{'`sed -e '/VERSION/!d' -e 's/.*"\(.*\)".*/\1/' -e q $?`'}' > $@

spell::		sp_errs
sp_errs:	$(TEX_SRC)
		$(REASON)
		$(DETEX) $(TEX_ROOT) | $(SPELL) > $@~
		mv $@~ $@

install-sh:
		ln $(TOP)/install-sh $@

$(man1dir):
		$(TOP)/mkinstalldirs $@

install::	$(MANUALS) install-sh $(man1dir)

install::
		for man in $(MANUALS.1); do\
		  $(INSTALL_DATA) $$man $(man1dir)/`basename $$man .1`.$(man1ext);\
		done
		for man in $(MANUALS.n); do\
		  $(INSTALL_DATA) $$man $(man1dir)/`basename $$man .n`.$(mannext);\
		done

clean::
		$(RM) *~ *.dvi *.log *.aux *.toc *.lof *.lot *.bbl *.blg sp_errs .emacs*

clobber::	clean

clobber::
		$(RM) *.ps

gen-distfiles::	$(DISTFILES)

distfiles::
		echo $(DISTFILES)

# dummy:
depend::
		@true
