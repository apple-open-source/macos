# c++-examples/snmp/makefile
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-examples/snmp/makefile,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:05  mb
# Move from private repository to open source repository
#
# Revision 1.3  2000/12/22 00:03:17  dmitch
# Misc. updates for clean build on Cheetah 1D7.
#
# Revision 1.2  2000/06/08 19:59:20  dmitch
# Mods for X port.
#
# Revision 1.1.1.1  1999/03/16 18:05:58  aram
# Originals from SMIME Free Library.
#
# Revision 1.10  1997/02/16 16:48:08  rj
# made return *this after calling abort()'' a compile time option.
#
# Revision 1.9  1995/07/27  11:59:01  rj
# rfc1213-mib -> rfc1213-mib2
#
# Revision 1.8  1995/07/27  08:29:58  rj
# rfc1155-smi.asn1, rfc1157-snmp.asn1 and rfc1213-mib2.asn1 renamed from 1155-smi.asn1, 1157-snmp.asn1 and 1213-mib2.asn1 to accomodate to snacc's new file name generation scheme.
#
# Revision 1.7  1995/07/24  15:30:11  rj
# `cd && make' instead of `cd; make'.
#
# changed `_' to `-' in file names.
#
# Revision 1.6  1995/02/20  11:48:41  rj
# build snacc if it doesn't exist.
#
# Revision 1.5  1995/02/13  14:58:15  rj
# augment CPPFLAGS, not overwrite
#
# Revision 1.4  1994/10/08  04:36:28  rj
# for the dependencies, generate the snacc compiler if it doesn't exist.
#
# Revision 1.3  1994/09/01  02:26:56  rj
# use CXX instead of CC to get another default linkage
#
# Revision 1.2  1994/08/31  23:25:41  rj
# print a less irritating usage.
#
# Revision 1.1  1994/08/31  08:48:14  rj
# first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
#

include ../../makehead

TOP		= ../..

ASN1_SPEC_DIR	= $(TOP)/asn1specs

ASN1_C++_LIB_DIR	= $(TOP)/c++-lib
ASN1_C++_INC_DIR	= $(ASN1_C++_LIB_DIR)/inc
ASN1_C++_LIB		= $(ASN1_C++_LIB_DIR)/libasn1c++.a

COMPILERDIR	= $(TOP)/compiler
SNACC		= $(COMPILERDIR)/snacc

CC		= $(CXX)
CPPFLAGS	+= -I$(TOP) -I$(ASN1_C++_INC_DIR) -F/System/Library/PrivateFrameworks

ASN1FILES = \
	$(ASN1_SPEC_DIR)/rfc1155-smi.asn1	\
	$(ASN1_SPEC_DIR)/rfc1157-snmp.asn1	\
	$(ASN1_SPEC_DIR)/rfc1213-mib2.asn1

ASN1HFILES = \
	rfc1155-smi.h	\
	rfc1213-mib2.h	\
	rfc1157-snmp.h

ASN1CFILES = \
	rfc1155-smi.cpp	\
	rfc1213-mib2.cpp	\
	rfc1157-snmp.cpp

DISTFILES = \
	README		\
	makefile	\

#-------------------------------------------------------------------------------

default::	warning

warning::
		@echo ''
		@echo 'read the README and enter `$(MAKE) fail` to test for yourself'
		@echo ''

fail::		$(ASN1HFILES) $(ASN1CFILES)
		$(CXX) $(CPPFLAGS) $(CFLAGS) -c $(ASN1CFILES)

$(ASN1HFILES)	\
$(ASN1CFILES):	$(SNACC) $(ASN1FILES)
		$(REASON)
		$(SNACC) -P -C $(SNACC_NOVOLAT) $(ASN1FILES) > snacc.output~
		mv snacc.output~ snacc.output

$(SNACC):
		cd $(@D) && $(MAKE) $(@F)

check::
		@echo "the purpose of this directories' contents is to show deficiencies."
		@echo "if you want to exercise 'em, make the phony target "'`'"fail'"

clean::
		$(RM) *.o *~ .emacs* core $(ASN1HFILES) $(ASN1CFILES) snacc.output

depend::	$(SNACC)

include ../../maketail
