# file: .../compiler/makefile
#
#	makefile for ASN.1 '88 parser
#
#  compiles the snacc ASN.1 compiler
#
# Mike Sample
# 1992
#
# $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/Attic/makefile,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:08  mb
# Move from private repository to open source repository
#
# Revision 1.2  2000/12/22 01:11:42  dmitch
# include ../makehead.static to workaround broken autoconf.
#
# Revision 1.1.1.1  1999/03/16 18:06:38  aram
# Originals from SMIME Free Library.
#
# Revision 1.12  1997/09/03 12:50:33  wan
# Shifted parse and lex
#
# Revision 1.11  1997/03/03 11:58:33  wan
# Final pre-delivery stuff (I hope).
#
# Revision 1.10  1997/02/28 13:39:52  wan
# Modifications collected for new version 1.3: Bug fixes, tk4.2.
#
# Revision 1.9  1997/02/16 18:02:49  rj
# snacc is needed to generate the distfiles
#
# Revision 1.8  1997/02/16 12:34:47  rj
# let make clobber remove the link to install-sh that make made
# let make clean remove more created files
#
# Revision 1.7  1997/01/02 08:47:20  rj
# an option to snacc added (when compiling the tbl.asn1 file)
#
# Revision 1.6  1995/07/25  18:09:18  rj
# the compiler is compiled from two files, tbl.h and tbl.c, it generates itself.
# for bootstrapping purposes, initial versions are supplied with the distribution.
#
# `cd && make' instead of `cd; make'.
#
# create installation directories only if they do not exist already.
#
# the compiler is compiled from two files, tbl.h and tbl.c, it generates itself.
# for bootstrapping purposes, initial versions are supplied with the distribution.
#
# changed `_' to `-' in file names.
#
# Revision 1.5  1995/02/20  11:53:57  rj
# distribute install-sh.
#
# Revision 1.4  1995/02/13  15:06:52  rj
# augment CPPFLAGS, not overwrite.
# CFLAGS moved to ../makehead.
#
# Revision 1.3  1994/10/08  03:47:23  rj
# added bootstrapping functionality for asn-useful.[ch].
#
# Revision 1.2  1994/09/01  00:08:49  rj
# gnu autoconf stuff added, DISTFILES, install phony target.
#
# Revision 1.1  1994/08/28  09:47:52  rj
# first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
#

include ../makehead
include ../makehead.static

TOP		= ..

ASN1_SRC_DIR	= $(TOP)/asn1specs
USEFUL_TYPES	= $(ASN1_SRC_DIR)/asn-useful.asn1

CORE_DIR	= core
BACKEND_DIR	= back-ends
C_BACKEND_DIR	= $(BACKEND_DIR)/c-gen
C++_BACKEND_DIR	= $(BACKEND_DIR)/c++-gen
IDL_BACKEND_DIR	= $(BACKEND_DIR)/idl-gen

ASN1_LIB_DIR	= ../c-lib
ASN1_INC_DIR	= $(ASN1_LIB_DIR)/inc
ASN1_BOOT_DIR	= $(ASN1_LIB_DIR)/boot
ASN1_LIB	= $(ASN1_LIB_DIR)/libasn1cCebuf.a

# some CFLAG options:
#  -DYYDEBUG use for yacc/lex debug info
#  -DDEBUG use to get general debug info

DEFS		= -DCOMPILER -DUSE_NIBBLE_MEMORY=0 -DUSE_EXP_BUF
INCLUDES	= -I$(CORE_DIR) -I$(ASN1_INC_DIR) -I$(ASN1_BOOT_DIR) -I$(BACKEND_DIR) -I$(TOP)
CPPFLAGS	+= $(DEFS) $(INCLUDES)

compilerFLAGS	= $(DEFS) -I../compiler/$(CORE_DIR)

LFILES	= \
	$(CORE_DIR)/lex-asn1.l
LCFILES	= \
	$(CORE_DIR)/lex-asn1.c

YFILES	= \
	$(CORE_DIR)/parse-asn1.y
YHFILES	= \
	$(CORE_DIR)/parse-asn1.h
YCFILES	= \
	$(CORE_DIR)/parse-asn1.c

HFILES.d = \
	$(CORE_DIR)/asn1module.h	\
	$(CORE_DIR)/define.h		\
	$(CORE_DIR)/dependency.h	\
	$(CORE_DIR)/do-macros.h	\
	$(CORE_DIR)/err-chk.h		\
	$(CORE_DIR)/exports.h		\
	$(CORE_DIR)/gen-tbls.h		\
	$(CORE_DIR)/lex-stuff.h	\
	$(CORE_DIR)/lib-types.h	\
	$(CORE_DIR)/link-types.h	\
	$(CORE_DIR)/link-values.h	\
	$(CORE_DIR)/mem.h		\
	$(CORE_DIR)/meta.h		\
	$(CORE_DIR)/normalize.h	\
	$(CORE_DIR)/oid.h		\
	$(CORE_DIR)/parser.h		\
	$(CORE_DIR)/print.h		\
	$(CORE_DIR)/recursive.h	\
	$(CORE_DIR)/snacc-util.h	\
	$(CORE_DIR)/tbl.h		\
	$(CORE_DIR)/val-parser.h	\
	\
	$(BACKEND_DIR)/str-util.h	\
	$(BACKEND_DIR)/tag-util.h	\
	$(BACKEND_DIR)/cond.h	\
	\
	$(C_BACKEND_DIR)/kwd.h		\
	$(C_BACKEND_DIR)/gen-any.h	\
	$(C_BACKEND_DIR)/gen-code.h	\
	$(C_BACKEND_DIR)/gen-dec.h	\
	$(C_BACKEND_DIR)/gen-enc.h	\
	$(C_BACKEND_DIR)/gen-free.h	\
	$(C_BACKEND_DIR)/gen-print.h	\
	$(C_BACKEND_DIR)/gen-type.h	\
	$(C_BACKEND_DIR)/gen-vals.h	\
	$(C_BACKEND_DIR)/rules.h		\
	$(C_BACKEND_DIR)/type-info.h	\
	$(C_BACKEND_DIR)/util.h		\
	\
	$(C++_BACKEND_DIR)/kwd.h	\
	$(C++_BACKEND_DIR)/rules.h	\
	$(C++_BACKEND_DIR)/types.h	\
	$(C++_BACKEND_DIR)/gen-any.h	\
	$(C++_BACKEND_DIR)/gen-code.h	\
	$(C++_BACKEND_DIR)/gen-vals.h	\
	\
	$(IDL_BACKEND_DIR)/rules.h	\
	$(IDL_BACKEND_DIR)/types.h	\
	$(IDL_BACKEND_DIR)/gen-any.h	\
	$(IDL_BACKEND_DIR)/gen-code.h	\
	$(IDL_BACKEND_DIR)/gen-vals.h

HFILES = \
	$(HFILES.d)		\
	$(CORE_DIR)/tbl.h

CFILES.d = \
	$(CORE_DIR)/define.c		\
	$(CORE_DIR)/dependency.c	\
	$(CORE_DIR)/do-macros.c		\
	$(CORE_DIR)/err-chk.c		\
	$(CORE_DIR)/exports.c		\
	$(CORE_DIR)/gen-tbls.c		\
	$(CORE_DIR)/lib-types.c		\
	$(CORE_DIR)/link-types.c	\
	$(CORE_DIR)/link-values.c	\
	$(CORE_DIR)/mem.c		\
	$(CORE_DIR)/meta.c		\
	$(CORE_DIR)/normalize.c		\
	$(CORE_DIR)/oid.c		\
	$(CORE_DIR)/print.c		\
	$(CORE_DIR)/recursive.c		\
	$(CORE_DIR)/snacc.c		\
	$(CORE_DIR)/snacc-util.c	\
	$(CORE_DIR)/val-parser.c	\
	\
	$(BACKEND_DIR)/str-util.c	\
	$(BACKEND_DIR)/tag-util.c	\
	$(BACKEND_DIR)/cond.c		\
	\
	$(C_BACKEND_DIR)/type-info.c	\
	$(C_BACKEND_DIR)/util.c		\
	$(C_BACKEND_DIR)/rules.c	\
	$(C_BACKEND_DIR)/gen-code.c	\
	$(C_BACKEND_DIR)/gen-type.c	\
	$(C_BACKEND_DIR)/gen-enc.c	\
	$(C_BACKEND_DIR)/gen-dec.c	\
	$(C_BACKEND_DIR)/gen-vals.c	\
	$(C_BACKEND_DIR)/gen-free.c	\
	$(C_BACKEND_DIR)/gen-print.c	\
	$(C_BACKEND_DIR)/gen-any.c	\
	$(C_BACKEND_DIR)/kwd.c		\
	\
	$(C++_BACKEND_DIR)/kwd.c	\
	$(C++_BACKEND_DIR)/types.c	\
	$(C++_BACKEND_DIR)/rules.c	\
	$(C++_BACKEND_DIR)/gen-code.c	\
	$(C++_BACKEND_DIR)/gen-vals.c	\
	$(C++_BACKEND_DIR)/gen-any.c	\
	\
	$(IDL_BACKEND_DIR)/rules.c	\
	$(IDL_BACKEND_DIR)/types.c	\
	$(IDL_BACKEND_DIR)/gen-any.c	\
	$(IDL_BACKEND_DIR)/gen-code.c	\
	$(IDL_BACKEND_DIR)/gen-vals.c

CFILES = \
	$(CFILES.d)		\
	$(CORE_DIR)/tbl.c

OFILES = \
	$(CORE_DIR)/parse-asn1.o	\
	$(CORE_DIR)/lex-asn1.o		\
	\
	$(CORE_DIR)/link-types.o	\
	$(CORE_DIR)/exports.o		\
	$(CORE_DIR)/snacc-util.o	\
	$(CORE_DIR)/dependency.o	\
	$(CORE_DIR)/lib-types.o	\
	$(CORE_DIR)/mem.o		\
	$(CORE_DIR)/meta.o		\
	$(CORE_DIR)/print.o		\
	$(CORE_DIR)/do-macros.o	\
	$(CORE_DIR)/oid.o		\
	$(CORE_DIR)/link-values.o	\
	$(CORE_DIR)/normalize.o	\
	$(CORE_DIR)/val-parser.o	\
	$(CORE_DIR)/err-chk.o		\
	$(CORE_DIR)/define.o		\
	$(CORE_DIR)/recursive.o	\
	$(CORE_DIR)/snacc.o		\
	$(CORE_DIR)/tbl.o		\
	$(CORE_DIR)/gen-tbls.o		\
	\
	$(BACKEND_DIR)/str-util.o	\
	$(BACKEND_DIR)/tag-util.o	\
	$(BACKEND_DIR)/cond.o		\
	\
	$(C_BACKEND_DIR)/type-info.o	\
	$(C_BACKEND_DIR)/util.o		\
	$(C_BACKEND_DIR)/rules.o	\
	$(C_BACKEND_DIR)/gen-code.o	\
	$(C_BACKEND_DIR)/gen-type.o	\
	$(C_BACKEND_DIR)/gen-enc.o	\
	$(C_BACKEND_DIR)/gen-dec.o	\
	$(C_BACKEND_DIR)/gen-vals.o	\
	$(C_BACKEND_DIR)/gen-free.o	\
	$(C_BACKEND_DIR)/gen-print.o	\
	$(C_BACKEND_DIR)/gen-any.o	\
	$(C_BACKEND_DIR)/kwd.o		\
	\
	$(C++_BACKEND_DIR)/kwd.o	\
	$(C++_BACKEND_DIR)/types.o	\
	$(C++_BACKEND_DIR)/rules.o	\
	$(C++_BACKEND_DIR)/gen-code.o	\
	$(C++_BACKEND_DIR)/gen-vals.o	\
	$(C++_BACKEND_DIR)/gen-any.o	\
	\
	$(IDL_BACKEND_DIR)/rules.o	\
	$(IDL_BACKEND_DIR)/types.o	\
	$(IDL_BACKEND_DIR)/gen-any.o	\
	$(IDL_BACKEND_DIR)/gen-code.o	\
	$(IDL_BACKEND_DIR)/gen-vals.o

DISTFILES = \
	README		\
	makefile	\
	$(HFILES.d)	\
	$(LFILES)	\
	$(YFILES)	\
	$(CFILES.d)	\
	boot/tbl.h	\
	boot/tbl.c

# end of definitions
# ----------------------------------------------------------------------
# rules start here

default::	all

all::		snacc

snacc:		$(OFILES) $(ASN1_LIB)
		$(REASON)
		$(CC) $(LDFLAGS) -o snacc $(OFILES) $(ASN1_LIB) $(LEXLIB) $(LIBS)

# compiling with the generated file currently doesn't work!
# otherwise, one could use the bootstrapping mechanism like for asn-useful.[ch] and tbl.[ch], below
#	$(CORE_DIR)/asn1module.h:	$(ASN1_SRC_DIR)/asn1module.asn1
#			./snacc -t -u $(USEFUL_TYPES) $(ASN1_SRC_DIR)/asn1module.asn1
#	# this file is empty, anyway:
#			$(RM) asn1module.c
#			mv asn1module.h $(CORE_DIR)

$(CORE_DIR)/tbl.h	\
$(CORE_DIR)/tbl.c::
		$(RM) $(CORE_DIR)/tbl.h $(CORE_DIR)/tbl.c
		if [ -f snacc ]; then\
		  $(MAKE) stamp-tbl;\
		else\
		  $(RM) tbl.h tbl.c;\
		  ln boot/tbl.h boot/tbl.c .;\
		fi
		ln tbl.h $(CORE_DIR)
		ln tbl.c $(CORE_DIR)

# `../compiler/snacc' instead of `snacc' or `./snacc' to trick make(1) into believing that both are different files to avoid infinitre recursion:
stamp-tbl:	../compiler/snacc $(USEFUL_TYPES) $(ASN1_SRC_DIR)/tbl.asn1 makefile
		$(REASON)
		-mv tbl.h tbl.h.prev
		-mv tbl.c tbl.c.prev
		./snacc -c -t -e -u $(USEFUL_TYPES) $(ASN1_SRC_DIR)/tbl.asn1
# the dates in the two files will differ, so the files really differ if diff prints more than 4 lines of output:
		@-if [ -f tbl.h.prev ]; then\
		  if [ `diff tbl.h.prev tbl.h | wc -l` -gt 4 ]; then\
		    $(RM) tbl.h.prev;\
		  else\
		    echo "tbl.h hasn't changed";\
		    mv tbl.h.prev tbl.h;\
		  fi;\
		fi
		@-if [ -f tbl.c.prev ]; then\
		  if [ `diff tbl.c.prev tbl.c | wc -l` -gt 4 ]; then\
		    $(RM) tbl.c.prev;\
		  else\
		    echo "tbl.c hasn't changed";\
		    mv tbl.c.prev tbl.c;\
		  fi;\
		fi
		date > $@

boot/tbl.h	\
boot/tbl.c:	stamp-tbl
		$(RM) boot/tbl.h boot/tbl.c
		cp -p tbl.h tbl.c boot

# the default rules work for .y.c, but often lack the .h
$(CORE_DIR)/parse-asn1.h	\
$(CORE_DIR)/parse-asn1.c:  $(CORE_DIR)/parse-asn1.y
		$(REASON)
		@echo "expect 61 shift/reduce and 2 reduce/reduce conflicts"
		$(YACC) -d $(CORE_DIR)/parse-asn1.y
		mv y.tab.h $(CORE_DIR)/parse-asn1.h
		mv y.tab.c $(CORE_DIR)/parse-asn1.c

$(ASN1_LIB)::
#		cd $(ASN1_LIB_DIR) && $(MAKE) Cebuf-lib compilerFLAGS="$(compilerFLAGS)"
# since different make versions handle the above line different, we use the more portable passing thru the environment:
		cd $(ASN1_LIB_DIR) && compilerFLAGS="$(compilerFLAGS)" $(MAKE) Cebuf-lib

install-sh:
		ln $(TOP)/install-sh $@

install::	snacc install-sh $(bindir)

$(bindir):
		$(TOP)/mkinstalldirs $@

install::
		$(INSTALL_PROGRAM) snacc $(bindir)/

clean::
		$(RM) $(OFILES) $(LCFILES) $(YHFILES) $(YCFILES)
		$(RM) *.o y.tab.c y.tab.h lex.yy.c y.output .emacs* *~ *.orig *.prev *.bak yacc.tmp yacc.acts yacc.debug
		$(RM) stamp-tbl tbl.h tbl.h.prev tbl.c tbl.c.prev

clobber::
		$(RM) snacc
		$(RM) install-sh

depend::	$(CORE_DIR)/tbl.h $(CORE_DIR)/tbl.c

gen-distfiles::	snacc

include ../maketail
