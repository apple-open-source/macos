# c++-examples/test-lib/makefile
#
# $Header: /cvs/root/Security/SecuritySNACCRuntime/c++-examples/test-lib/Attic/makefile,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:05  mb
# Move from private repository to open source repository
#
# Revision 1.2  2000/12/22 00:03:29  dmitch
# Misc. updates for clean build on Cheetah 1D7.
#
# Revision 1.1.1.1  1999/03/16 18:05:58  aram
# Originals from SMIME Free Library.
#
# Revision 1.7  1997/03/03 11:58:30  wan
# Final pre-delivery stuff (I hope).
#
# Revision 1.6  1997/02/28 13:39:41  wan
# Modifications collected for new version 1.3: Bug fixes, tk4.2.
#
# Revision 1.5  1995/07/24 15:31:14  rj
# changed `_' to `-' in file names.
#
# Revision 1.4  1995/02/13  14:58:17  rj
# augment CPPFLAGS, not overwrite
#
# Revision 1.3  1994/09/01  02:26:58  rj
# use CXX instead of CC to get another default linkage
#
# Revision 1.2  1994/08/31  21:44:25  rj
# rebuild the executables when the c++-lib is newer.
#
# Revision 1.1  1994/08/31  08:48:17  rj
# first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
#

include ../../makehead

TOP		= ../..

ASN1_C++_LIB_DIR	= $(TOP)/c++-lib
ASN1_C++_INC_DIR	= $(ASN1_C++_LIB_DIR)/inc
ASN1_C++_LIB		= $(ASN1_C++_LIB_DIR)/libasn1c++.a

CC		= $(CXX)
CPPFLAGS	+= -I$(TOP) -I$(ASN1_C++_INC_DIR) -F/System/Library/PrivateFrameworks

CFILES = \
	test-lib.C

DISTFILES = \
	README		\
	makefile	\
	$(CFILES)

#-------------------------------------------------------------------------------

default::	check

check::		test-lib
		./test-lib

test-lib:	$(ASN1_C++_LIB) test-lib.o
		$(CXX) $(LDFLAGS) -o $@ test-lib.o $(ASN1_C++_LIB) $(LIBS) -lstdc++

clean::
		$(RM) *.o *~ .emacs* test-lib core

include ../../maketail
