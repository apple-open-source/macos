#
# Makefile for the snaccwish
#
# $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/tcl-example/makefile,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:55  aram
# Originals from SMIME Free Library.
#
# Revision 1.2  1997/02/16 12:53:24  rj
# remove more generated files
#
# Revision 1.1  1997/01/01 22:57:14  rj
# first check-in
#

include ../makehead

TOP		= ..

SNACC		= $(TOP)/compiler/snacc

ASN1_SRC_DIR	= $(TOP)/asn1specs
C++_LIB		= $(TOP)/c++-lib
ASN1INC		= $(C++_LIB)/inc
ASN1LIB		= $(C++_LIB)/libasn1tcl.a
TCL_LIB		= $(TOP)/tcl-lib

CPPFLAGS	+= -I$(TOP) -I$(ASN1INC) $(CPPFLAGS.TCL) $(CPPFLAGS.X11) -DTCL -DSNACC_DEEP_COPY

PDUS		= X.T1,X.T4
PDUS		= EdEx-Structured.File,EdEx-Structured.Directory,EdEx-Structured.Various

ASN1FILES	= \
	edex0.asn1	\
	edex1.asn1

ASN1HFILES	= \
	edex0.h		\
	edex1.h

ASN1CFILES	= \
	edex0.C		\
	edex1.C		\
	modules.C

ASN1OFILES	= \
	edex0.o		\
	edex1.o		\
	modules.o

OFILES	= \
	$(ASN1OFILES)

DISTFILES = \
	makefile	\
	$(ASN1FILES)

LDFLAGS		+= $(LDFLAGS.X11)
LDLIBS		+= $(ASN1LIB) $(TREELIBS) $(TCLLIBS) $(LDLIBS.X11pre) -lX11 $(LDLIBS.X11extra) $(LIBS)

WISH		= snaccwish
SCRIPT		= snacced

#-------------------------------------------------------------------------------

all::		$(WISH) $(SCRIPT)

$(WISH):	$(OFILES) $(ASN1LIB)
		$(REASON)
		$(CXX) $(LDFLAGS) -o $(WISH) $(OFILES) $(LDLIBS)

$(SCRIPT)::
		$(TCL_LIB)/make-snacced $@ ./$(WISH) $(TCL_LIB)

$(ASN1LIB)::
		cd $(@D) && $(MAKE) tcl-lib

# if $(ASN1INC)/asn_useful.h doesn't exist, someone probably called `make clean` in $(C++_LIB).
$(ASN1INC)/asn-useful.h:
		$(MAKE) $(ASN1LIB)

$(ASN1HFILES) $(ASN1CFILES):	$(SNACC) $(ASN1FILES) makefile
		$(REASON)
		$(SNACC) -u $(ASN1_SRC_DIR)/asn-useful.asn1 -tcl $(PDUS) $(ASN1FILES)

$(SNACC)::
		cd $(@D) && $(MAKE) $(@F)

clean::
		$(RM) $(ASN1HFILES) $(ASN1CFILES)
		$(RM) $(OFILES)

clobber::
		$(RM) $(WISH)

# dummy:
install::

include ../maketail
