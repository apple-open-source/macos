# file: .../tbl-example/makefile
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/tbl-example/makefile,v 1.1.1.1 2001/05/18 23:14:10 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:10  mb
# Move from private repository to open source repository
#
# Revision 1.1.1.1  1999/03/16 18:06:53  aram
# Originals from SMIME Free Library.
#
# Revision 1.1  1997/02/15 19:33:26  rj
# first check-in
#

include ../makehead

TOP		= ..

ASN1_SRC_DIR	= $(TOP)/asn1specs

ASN1_C_LIB_DIR	= $(TOP)/c-lib
ASN1_C_INC_DIR	= $(ASN1_C_LIB_DIR)/inc
TTBL_ASN1_LIB	= $(ASN1_C_LIB_DIR)/libasn1ctbl.a

COMPILERDIR	= $(TOP)/compiler
SNACC		= $(COMPILERDIR)/snacc
USEFUL_TYPES	= $(ASN1_SRC_DIR)/asn-useful.asn1
SNACCFLAGS	= -u $(USEFUL_TYPES)

TBLTOOLSDIR	= $(TOP)/tbl-tools
MKHDR		= $(TBLTOOLSDIR)/mkchdr/mkchdr

ASN1FILES	= $(ASN1_SRC_DIR)/p-rec.asn1
ASN1TTFILE	= p-rec.tt
ASN1TTHFILE	= p-rec.h

CFILES	= example.c
OFILES	= example.o

DISTFILES = \
	README		\
	makefile	\
	$(CFILES)	\
	p-rec.ber

CPPFLAGS	+= -I$(TOP) -I$(ASN1_C_INC_DIR) -DUSE_GEN_BUF

#-------------------------------------------------------------------------------

all::		example

example:	$(OFILES) $(TTBL_ASN1_LIB)
		$(REASON)
		$(CC) $(CFLAGS) -o example $(OFILES) $(TTBL_ASN1_LIB) $(LIBS)

$(ASN1TTFILE):	$(ASN1FILES)
		$(REASON)
		$(SNACC) $(SNACCFLAGS) -T $@ $(ASN1FILES)

$(SNACC) $(MKHDR):
		cd $(@D) && $(MAKE) $(@F)

$(TTBL_ASN1_LIB):
		cd $(@D) && $(MAKE) tbl-lib

$(ASN1TTHFILE):	$(ASN1TTFILE)
		$(REASON)
		$(MKHDR) $? $@

check::		example $(ASN1TTFILE)

check::
		./example p-rec.tt p-rec.ber
		cmp p-rec.ber p-rec.out.ber
		@echo "+++ the test was successful"

# dummy:
install::

clean::
		$(RM) example *.o *~ core .emacs* $(ASN1TTFILE) $(ASN1TTHFILE) p-rec.out.ber

depend::
		$(MAKE) $(MKHDR)

depend::	$(ASN1TTHFILE)

include ../maketail
