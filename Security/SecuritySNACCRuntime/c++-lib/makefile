# c++-lib/makefile
#
#  makes C++ ASN.1 runtime library
#
# MS 92
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-lib/makefile,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:05  mb
# Move from private repository to open source repository
#
# Revision 1.3  2000/12/22 00:04:15  dmitch
# Misc. updates for clean build on Cheetah 1D7.
#
# Revision 1.2  2000/05/10 23:38:06  rmurphy
# Updating makefile  and install related files
#
# Revision 1.1.1.1  1999/03/16 18:05:58  aram
# Originals from SMIME Free Library.
#
# Revision 1.12  1997/03/13 09:15:05  wan
# Improved dependency generation for stupid makedepends.
# Corrected PeekTag to peek into buffer only as far as necessary.
# Added installable error handler.
# Fixed small glitch in idl-code generator (Markku Savela <msa@msa.tte.vtt.fi>).
#
# Revision 1.11  1997/02/16 16:48:25  rj
# made return *this after calling abort()'' a compile time option.
#
# Revision 1.10  1997/02/15 20:40:26  rj
# clean/clobber simplified, no recursive call
#
# Revision 1.9  1995/09/07  18:44:09  rj
# snacced.[hC] renamed into tcl-if.[hC],
# tkXAppInit.c lost its X.
#
# asn-useful.asn1 is compiled with the -l 50 option, as recommended by MS in the documentation.
#
# Revision 1.8  1995/07/27  08:33:31  rj
# make $(TCL-P) if necessary.
#
# actions for stamp-useful rule: call diff only if both input files exist.
#
# Revision 1.7  1995/07/24  15:52:59  rj
# changed `_' to `-' in file names.
#
# useful.asn1 renamed to asn-useful.asn1 to accomodate to snacc's new file name generation scheme.
#
# make tcl interface as determined thru the $(TCL-P) utility prog.
#
# `cd && make' instead of `cd; make'.
#
# create installation directories only if they do not exist already.
#
# call make recursively to generate the libraries different dependencies and concatenate them.
#
# Revision 1.6  1995/02/20  11:40:24  rj
# remove stamp-useful before removing asn-useful.[hC].
# distribute install-sh.
# use $(INSTALL*) in a for-loop as some install programs (namely the distributed install-sh) only process their first argument!
#
# Revision 1.5  1995/02/09  14:03:22  rj
# enforce generation of inc/asn-useful.h and src/asn-useful.cpp when making dependencies
#
# Revision 1.4  1994/10/08  05:27:31  rj
# don't distribute the non-bootstrapping version of asn-useful.[hc]
#
# Revision 1.3  1994/10/08  04:03:19  rj
# a new asn-useful.[Ch] get generated every time the compiler or asn-useful.asn1 changes.
#
# the Tcl interface needs a separate library because conditional compilation and virtual tables don't match otherwise.
#
# Revision 1.2  1994/08/31  23:28:41  rj
# DISTFILES and install phony target added.
#
# Revision 1.1  1994/08/28  09:20:22  rj
# first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
#

include ../makehead
include ../makehead.static

DEPENDENCIES	=

TOP		= ..

ASN1_SRC_DIR	= $(TOP)/asn1specs

COMPILERDIR	= $(TOP)/compiler
SNACC		= $(COMPILERDIR)/snacc
USEFUL_TYPES	= $(ASN1_SRC_DIR)/asn-useful.asn1

CPPFLAGS	+= -Iinc -I$(TOP) $(tcl_def) -F/System/Library/PrivateFrameworks

#
# hack around broken autoconf and missing makehead
#
RANLIB= /usr/bin/ranlib
TCL-P = $(TOP)/tcl-p

#- #ifdef _IBM_ENC_
#- # Base directory of snacc11
#- UserDir=/net/saturn/usr4/GLASS/tools
#- Snacc11Dir=$(UserDir)/snacc
#- #
#- # where ShmMgr is located
#- ShmMgrDir=/net/saturn/usr4/GLASS/tools/shmmgr
#- #
#- # Directories of the HeiTS
#- # needed to include *.h files and Libs.
#- HeiTSDir = /u/HeiTS
#- OSSDir = $(HeiTSDir)/oss
#- BMSDir = $(HeiTSDir)/bms
#- MEMDir = $(HeiTSDir)/memmgr
#- #
#- # my own .h files for OSS are located at
#- OwnOSSDir = $(UserDir)/include/HeiTS3.2.6/oss_aix
#- OwnMemMgrDir = $(UserDir)/include/HeiTS3.2.6/memmgr
#- #
#- # OSS include and define for compiling
#- # HeiTS 3.2.6
#- HeiTSIncDirs = -I$(OwnOSSDir) -I$(OwnMemMgrDir) -D__AIX_32__ -D__AIX__
#- # HeiTS 3.2.7
#- #HeiTSIncDirs = -I$(OwnOSSDir) -I$(OwnMemMgrDir) -D__ALL_SOURCE -D__AIX_32__ -DMMSERV -DDEBUG -DMAP_MANAGEMENT -DMHEG
#- #
#- LIB_HDR_DIR = $(Snacc11Dir)/cpp_incl
#-
#- CC = xlC
#- CFLAGS = -c -g -qchars=signed -+ -DIEEE_REAL_FMT -I$(LIB_HDR_DIR) -I$(ShmMgrDir) $(HeiTSIncDirs)
#- #endif /* _IBM_ENC_ */

HFILES.d = \
	inc/asn-any.h		\
	inc/asn-bits.h		\
	inc/asn-bool.h		\
	inc/asn-buf.h		\
	inc/asn-config.h	\
	inc/asn-enum.h		\
	inc/asn-incl.h		\
	inc/asn-int.h		\
	inc/asn-len.h		\
	inc/asn-list.h		\
	inc/asn-null.h		\
	inc/asn-octs.h		\
	inc/asn-oid.h		\
	inc/asn-real.h		\
	inc/asn-tag.h		\
	inc/asn-type.h		\
	inc/hash.h		\
	inc/init.h		\
	inc/meta.h		\
	inc/print.h		\
	inc/tcl-if.h		\
	inc/str-stk.h

HFILES = \
	$(HFILES.d)		\
	inc/asn-useful.h

HFILES.shared = \
	$(TOP)/snacc.h		\
	$(TOP)/config.h		\
	$(TOP)/policy.h

CFILES.d = \
	src/asn-any.cpp		\
	src/asn-bits.cpp		\
	src/asn-bool.cpp		\
	src/asn-enum.cpp		\
	src/asn-int.cpp		\
	src/asn-len.cpp		\
	src/asn-list.cpp		\
	src/asn-null.cpp		\
	src/asn-octs.cpp		\
	src/asn-oid.cpp		\
	src/asn-real.cpp		\
	src/asn-tag.cpp		\
	src/asn-type.cpp		\
	src/hash.cpp		\
	src/meta.cpp		\
	src/print.cpp		\
	src/tcl-if.cpp		\
	src/str-stk.cpp		\
	src/tkAppInit.c

CFILES = \
	$(CFILES.d)		\
	src/asn-useful.cpp

d	= src

OFILES = \
	$d/asn-any.o		\
	$d/asn-bits.o		\
	$d/asn-bool.o		\
	$d/asn-enum.o		\
	$d/asn-int.o		\
	$d/asn-len.o		\
	$d/asn-list.o		\
	$d/asn-null.o		\
	$d/asn-octs.o		\
	$d/asn-oid.o		\
	$d/asn-real.o		\
	$d/asn-tag.o		\
	$d/asn-type.o		\
	$d/asn-useful.o		\
	$d/hash.o		\
	$d/meta.o		\
	$d/print.o		\
	$d/tcl-if.o		\
	$d/str-stk.o		\
	$d/tkAppInit.o

DISTFILES = \
	README		\
	makefile	\
	$(HFILES.d)	\
	$(CFILES.d)

LIB		= libasn1c++.a

#-------------------------------------------------------------------------------

default::	c++-lib tcl-lib

d = dummy

$d::		$(CFILES)

$d::
		test -d $@ || mkdir $@
		$(RM) $@/*.[Cc] $@/*.cpp
		ln $(CFILES) $@

.PHONY:	ofiles lib
.PHONY:	c++-lib tcl-lib

ofiles:		$(OFILES)

lib:		libasn1$d.a

c++-lib:
		$(MAKE) d=c++ c++ ofiles lib tcl_def=-DMETA=0

tcl-lib::	$(TCL-P)

tcl-lib::
		if $(TCL-P); then $(MAKE) d=tcl tcl ofiles lib tcl_def=-DTCL; fi

libasn1c++.a	\
libasn1tcl.a:	$(OFILES)
		$(AR) rv $@ $?
		$(RANLIB) $@

#$(LIB)::	$(CFILES)

inc/asn-useful.h	\
src/asn-useful.cpp:	stamp-useful

stamp-useful:	$(SNACC) $(USEFUL_TYPES) $(TCL-P)
		if $(TCL-P); then\
		  $(SNACC) -C $(SNACC_NOVOLAT) -l 50 -tcl "" $(USEFUL_TYPES);\
		  $(RM) modules.cpp;\
		else\
		  $(SNACC) -C $(SNACC_NOVOLAT) -l 50 $(USEFUL_TYPES);\
		fi
# the dates in the two files will differ, so the files really differ if diff prints more than 4 lines of output:
		@if [ ! -f inc/asn-useful.h ] || [ `diff asn-useful.h inc/asn-useful.h | wc -l` -gt 4 ]; then\
		  mv asn-useful.h inc/asn-useful.h;\
		else\
		  echo "asn-useful.h hasn't changed";\
		  $(RM) asn-useful.h;\
		fi
		@if [ ! -f src/asn-useful.cpp ] || [ `diff asn-useful.cpp src/asn-useful.cpp | wc -l` -gt 4 ]; then\
		  mv asn-useful.cpp src/asn-useful.cpp;\
		else\
		  echo "asn-useful.cpp hasn't changed";\
		  $(RM) asn-useful.cpp;\
		fi
		date > $@

$(SNACC):
		cd $(@D) && $(MAKE) $(@F)

install-sh:
		ln $(TOP)/install-sh $@

$(incdir)/snacc/c++ $(libexecdir):
		$(TOP)/mkinstalldirs $@

install::	$(HFILES) c++-lib tcl-lib install-sh $(incdir)/snacc/c++ $(libexecdir)

install::
		for h in $(HFILES) $(HFILES.shared); do $(INSTALL_DATA) $$h $(incdir)/snacc/c++/; done
		for l in $(LIB); do $(INSTALL_DATA) $$l $(libexecdir)/; $(RANLIB) -t $(libexecdir)/$$l; done
		if [ $d != tcl ]; then $(MAKE) LIB=libasn1tcl.a d=tcl $@; fi

clean::
		$(RM) stamp-useful
		$(RM) asn-useful.h inc/asn-useful.h
		$(RM) asn-useful.cpp src/asn-useful.cpp
		$(RM) $(OFILES)
		$(RM) -r c++ tcl
		$(RM) *.o *~ .emacs* core

clobber::
		$(RM) libasn1c++.a
		$(RM) libasn1tcl.a
		$(RM) install-sh

depend::	inc/asn-useful.h src/asn-useful.cpp

depend::
		if [ x"$(DEPENDENCIES)" = x ]; then\
		  $(MAKE) d=c++ c++ DEPENDENCIES=deps.c++ depend;\
		  if $(TCL-P); then\
		    $(MAKE) d=tcl tcl tcl_def=-DTCL DEPENDENCIES=deps.tcl depend;\
		  fi;\
		  cp /dev/null dependencies;\
		  for dir in c++ tcl; do\
		    < deps.$$dir sed -e "s/^src/$$dir/;s/^[a-z-]*\.o:/$$dir\/&/" >> dependencies;\
		    $(RM) deps.$$dir;\
		  done;\
		fi

include ../maketail
