# file: .../tcl-lib/makefile
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/tcl-lib/makefile,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:55  aram
# Originals from SMIME Free Library.
#
# Revision 1.1  1997/01/01 23:11:57  rj
# first check-in
#

include ../makehead

TOP		= ..

TCLFILES.dist	= \
	bindings.tcl	\
	help.tcl	\
	selbox.tcl	\
	snacced.tcl	\
	tkuti.tcl	\
	uti.tcl

TCLFILES	= \
	$(TCLFILES.dist)	\
	tclIndex

DISTFILES	= \
	$(TCLFILES.dist)	\
	make-snacced	\
	makefile

default:	tclIndex

tclIndex:	$(TCLFILES.dist) makefile
		echo 'auto_mkindex . $(TCLFILES.dist)' | $(TCLSH)

install-sh:
		ln $(TOP)/install-sh $@

install::	$(TCLFILES) install-sh $(tcldir)

$(tcldir):
		$(TOP)/mkinstalldirs $@

install::
		$(INSTALL_PROGRAM) make-snacced	$(bindir)/
		for tcl in $(TCLFILES); do\
		  $(INSTALL_DATA) $$tcl $(tcldir)/;\
		done

clean::
		$(RM) *~ *.dvi *.log *.aux *.toc *.lof *.lot *.bbl *.blg sp_errs .emacs*

clobber::
		$(RM) tclIndex
		$(RM) install-sh

gen-distfiles::	$(DISTFILES)

distfiles::
		echo $(DISTFILES)

# dummy:
depend::
		@true
