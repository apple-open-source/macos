# c-lib/makefile
#
# makes C ASN.1 runtime libraries (one for each flavour of buffer)
#
# MS 92
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/makefile,v 1.1.1.1 2001/05/18 23:14:07 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:07  mb
# Move from private repository to open source repository
#
# Revision 1.3  2000/12/22 01:22:41  dmitch
# Misc. mods for clean build on Cheetah 1D7.
#
# Revision 1.2  2000/06/08 20:07:49  dmitch
# Mods for X port.
#
# Revision 1.1.1.1  1999/03/16 18:06:10  aram
# Originals from SMIME Free Library.
#
# Revision 1.12  1997/08/27 15:55:30  wan
# Added generic table decoding, debug routines, berdecode, and asnwish.
#
# Revision 1.11  1997/03/13 09:15:08  wan
# Improved dependency generation for stupid makedepends.
# Corrected PeekTag to peek into buffer only as far as necessary.
# Added installable error handler.
# Fixed small glitch in idl-code generator (Markku Savela <msa@msa.tte.vtt.fi>).
#
# Revision 1.10  1997/02/16 12:34:03  rj
# let make clobber remove the link to install-sh that make made
#
# Revision 1.9  1995/09/07 18:58:15  rj
# asn-useful.asn1 is compiled with the -l 50 option, as recommended by MS in the documentation
#
# Revision 1.8  1995/07/24  20:56:31  rj
# useful.asn1 renamed to asn-useful.asn1 to accomodate to snacc's new file name generation scheme.
#
# generate tbl.[hc] from .../asn1specs/tbl.asn1
# patch tbl.h to add some data members.
# distribute boot versions of tbl.[ch] and tbl.h.path.
#
# generate type table library.
#
# create installation directories only if they do not exist already.
#
# ranlib the copied libraries.
#
# call make recursively to generate the libraries different dependencies and concatenate them.
#
# changed `_' to `-' in file names.
#
# Revision 1.7  1995/02/20  11:42:04  rj
# remove stamp-useful before removing asn-useful.[hc].
# distribute install-sh.
# use $(INSTALL*) in a for-loop as some install programs (namely the distributed install-sh) only process their first argument!
#
# Revision 1.6  1995/02/09  14:02:47  rj
# enforce generation of inc/asn-useful.h and src/asn-useful.c when making dependencies
#
# Revision 1.5  1994/10/08  05:27:56  rj
# typo fixed.
#
# Revision 1.4  1994/10/08  05:20:07  rj
# don't distribute the non-bootstrapping version of asn-useful.[hc]
#
# Revision 1.3  1994/10/08  04:47:42  rj
# the files asn-useful.[hc] get regenerated automagically. since the compiler itself is compiled with it, a bootstapping version is included in the distribution.
#
# Revision 1.2  1994/08/31  23:51:42  rj
# DISTFILES and install phony target added.
#
# Revision 1.1  1994/08/28  09:21:20  rj
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

CPPFLAGS	+= -I$(TOP) -Iinc $(bufFLAGS) $(compilerFLAGS)

RANLIB= /usr/bin/ranlib

HFILES.d = \
	inc/asn-any.h		\
	inc/asn-bits.h		\
	inc/asn-bool.h		\
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
	inc/exp-buf.h		\
	inc/gen-buf.h		\
	inc/hash.h		\
	inc/min-buf.h		\
	inc/nibble-alloc.h	\
	inc/print.h		\
	inc/sbuf.h		\
	inc/str-stk.h		\
	\
	inc/tbl-dec.h		\
	inc/tbl-enc.h		\
	inc/tbl-free.h		\
	inc/tbl-gen.h		\
	inc/tbl-dbg.h		\
	inc/tbl-gen-c-hdr.h	\
	inc/tbl-incl.h		\
	inc/tbl-print.h		\
	inc/tbl-util.h

HFILES = \
	$(HFILES.d)		\
	inc/asn-useful.h	\
	inc/tbl.h

HFILES.shared = \
	$(TOP)/snacc.h		\
	$(TOP)/config.h		\
	$(TOP)/policy.h

CFILES.d = \
	src/asn-len.c		\
	src/asn-tag.c		\
	src/asn-int.c		\
	src/asn-bool.c		\
	src/asn-bits.c		\
	src/asn-octs.c		\
	src/asn-oid.c		\
	src/asn-real.c		\
	src/asn-null.c		\
	src/asn-list.c		\
	src/asn-enum.c		\
	src/str-stk.c		\
	src/nibble-alloc.c	\
	src/print.c		\
	src/asn-any.c		\
	src/hash.c		\
	src/exp-buf.c		\
	src/sbuf.c		\
	\
	src/tbl-dec.c		\
	src/tbl-enc.c		\
	src/tbl-free.c 		\
	src/tbl-gen.c		\
	src/tbl-dbg.c		\
	src/tbl-print.c		\
	src/tbl-util.c

CFILES = \
	$(CFILES.d)		\
	src/asn-useful.c	\
	src/tbl.c

OFILES = \
	$b/asn-len.o		\
	$b/asn-tag.o		\
	$b/asn-int.o		\
	$b/asn-bool.o		\
	$b/asn-bits.o		\
	$b/asn-octs.o		\
	$b/asn-oid.o		\
	$b/asn-real.o		\
	$b/asn-null.o		\
	$b/asn-list.o		\
	$b/asn-useful.o		\
	$b/asn-enum.o		\
	$b/str-stk.o		\
	$b/nibble-alloc.o	\
	$b/print.o		\
	$b/asn-any.o		\
	$b/hash.o		\
	$b/exp-buf.o		\
	$b/sbuf.o		\
	\
	$b/tbl.o		\
	$b/tbl-dec.o		\
	$b/tbl-enc.o		\
	$b/tbl-free.o 		\
	$b/tbl-gen.o		\
	$b/tbl-dbg.o		\
	$b/tbl-print.o		\
	$b/tbl-util.o

DISTFILES = \
	README		\
	makefile	\
	$(HFILES.d)	\
	$(CFILES.d)	\
	boot/asn-useful.h	\
	boot/asn-useful.c	\
	tbl.h.patch	\
	boot/tbl.h	\
	boot/tbl.c

EXPORTLIBS	= libasn1cebuf.a libasn1cmbuf.a libasn1csbuf.a libasn1ctbl.a
compilerLIB	= libasn1cCebuf.a

LIBS	= \
	$(EXPORTLIBS)	\
	$(compilerLIB)

MAKEFLAGS.ebuf	= bufFLAGS=-DUSE_EXP_BUF b=ebuf ebuf
MAKEFLAGS.Cebuf	= bufFLAGS=-DUSE_EXP_BUF b=Cebuf Cebuf
MAKEFLAGS.mbuf	= bufFLAGS=-DUSE_MIN_BUF b=mbuf mbuf
MAKEFLAGS.sbuf	= bufFLAGS=-DUSE_SBUF b=sbuf sbuf
MAKEFLAGS.tbl	= bufFLAGS="-DUSE_GEN_BUF -DTTBL" b=tbl tbl

#-------------------------------------------------------------------------------

.PHONY:	ebuf Cebuf mbuf sbuf tbl

default::	ebuf-lib mbuf-lib sbuf-lib tbl-lib
		@echo "C ASN.1 libraries made"

all::		default Cebuf-lib

b = dummy

$b::		$(CFILES)

$b::
		test -d $@ || mkdir $@
		$(RM) $@/*.c
		-ln $(CFILES) $@

.PHONY:	ofiles lib
.PHONY:	ebuf-lib Cebuf-lib mbuf-lib sbuf-lib tbl-lib

ofiles:		$(OFILES)

lib:		libasn1c$b.a

ebuf-lib:
		$(MAKE) $(MAKEFLAGS.ebuf) ofiles lib

Cebuf-lib:
		$(MAKE) $(MAKEFLAGS.Cebuf) ofiles lib

mbuf-lib:
		$(MAKE) $(MAKEFLAGS.mbuf) ofiles lib

sbuf-lib:
		$(MAKE) $(MAKEFLAGS.sbuf) ofiles lib

tbl-lib:
		$(MAKE) $(MAKEFLAGS.tbl) ofiles lib

libasn1cebuf.a	\
libasn1cCebuf.a	\
libasn1cmbuf.a	\
libasn1csbuf.a	\
libasn1ctbl.a:	$(OFILES)
		$(AR) rv $@ $?
		$(RANLIB) $@

inc/asn-useful.h	\
src/asn-useful.c::
		$(RM) inc/asn-useful.h src/asn-useful.c
		if [ -f $(SNACC) ]; then\
		  $(MAKE) stamp-useful;\
		else\
		  $(RM) asn-useful.h asn-useful.c;\
		  ln boot/asn-useful.h boot/asn-useful.c .;\
		fi
		ln asn-useful.h inc
		ln asn-useful.c src

stamp-useful:	$(SNACC) $(USEFUL_TYPES) makefile
		-mv asn-useful.h asn-useful.h.prev
		-mv asn-useful.c asn-useful.c.prev
		$(SNACC) -c -l 50 $(USEFUL_TYPES)
# the dates in the two files will differ, so the files really differ if diff prints more than 4 lines of output:
		@if [ -f asn-useful.h.prev ]; then\
		  if [ `diff asn-useful.h.prev asn-useful.h | wc -l` -gt 4 ]; then\
		    $(RM) asn-useful.h.prev;\
		  else\
		    echo "asn-useful.h hasn't changed";\
		    mv asn-useful.h.prev asn-useful.h;\
		  fi;\
		fi
		@if [ -f asn-useful.c.prev ]; then\
		  if [ `diff asn-useful.c.prev asn-useful.c | wc -l` -gt 4 ]; then\
		    $(RM) asn-useful.c.prev;\
		  else\
		    echo "asn-useful.c hasn't changed";\
		    mv asn-useful.c.prev asn-useful.c;\
		  fi;\
		fi
		date > $@

boot/asn-useful.h	\
boot/asn-useful.c:	stamp-useful
		$(RM) boot/asn-useful.h boot/asn-useful.c
		cp -p asn-useful.h asn-useful.c boot

inc/tbl.h	\
src/tbl.c::
		$(RM) inc/tbl.h src/tbl.c
		if [ -f $(SNACC) ]; then\
		  $(MAKE) stamp-tbl;\
		else\
		  $(RM) tbl.h tbl.c;\
		  ln boot/tbl.h boot/tbl.c .;\
		fi
		ln tbl.h inc
		ln tbl.c src

stamp-tbl:	$(SNACC) $(USEFUL_TYPES) $(ASN1_SRC_DIR)/tbl.asn1 makefile
		-mv tbl.h tbl.h.prev
		-mv tbl.c tbl.c.prev
		$(SNACC) -u $(USEFUL_TYPES) $(ASN1_SRC_DIR)/tbl.asn1
		$(PATCH) < tbl.h.patch
		echo '#if TTBL' > tbl.c.tmp
		cat tbl.c >> tbl.c.tmp
		echo '#endif /* TTBL */' >> tbl.c.tmp
		mv tbl.c.tmp tbl.c
# the dates in the two files will differ, so the files really differ if diff prints more than 4 lines of output:
		@if [ -f tbl.h.prev ]; then\
		  if [ `diff tbl.h.prev tbl.h | wc -l` -gt 4 ]; then\
		    $(RM) tbl.h.prev;\
		  else\
		    echo "tbl.h hasn't changed";\
		    mv tbl.h.prev tbl.h;\
		  fi;\
		fi
		@if [ -f tbl.c.prev ]; then\
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

install-sh:
		ln $(TOP)/install-sh $@

$(incdir)/snacc/c $(libdir):
		$(TOP)/mkinstalldirs $@

install::	$(HFILES) ebuf-lib mbuf-lib sbuf-lib tbl-lib install-sh $(incdir)/snacc/c $(libdir)

install::
		for h in $(HFILES) $(HFILES.shared); do $(INSTALL_DATA) $$h $(incdir)/snacc/c/; done
		for l in $(EXPORTLIBS); do $(INSTALL_DATA) $$l $(libexecdir)/; $(RANLIB) -t $(libexecdir)/$$l; done

clean::
		$(RM) *.o *~ *.prev .emacs* core
		$(RM) stamp-useful stamp-tbl
		$(RM) asn-useful.h inc/asn-useful.h
		$(RM) asn-useful.c src/asn-useful.c
		$(RM) tbl.h inc/tbl.h
		$(RM) tbl.c src/tbl.c
		$(RM) -r ebuf Cebuf mbuf sbuf tbl
		$(RM) $(compilerLIB)

clobber::
		$(RM) $(LIBS)
		$(RM) install-sh

depend::	inc/asn-useful.h src/asn-useful.c
depend::	inc/tbl.h src/tbl.c

depend::
		if [ x"$(DEPENDENCIES)" = x ]; then\
		  $(MAKE) $(MAKEFLAGS.ebuf) DEPENDENCIES=deps.ebuf depend;\
		  $(MAKE) $(MAKEFLAGS.Cebuf) DEPENDENCIES=deps.Cebuf depend;\
		  $(MAKE) $(MAKEFLAGS.mbuf) DEPENDENCIES=deps.mbuf depend;\
		  $(MAKE) $(MAKEFLAGS.sbuf) DEPENDENCIES=deps.sbuf depend;\
		  $(MAKE) $(MAKEFLAGS.tbl) DEPENDENCIES=deps.tbl depend;\
		  cp /dev/null dependencies;\
		  for dir in ebuf Cebuf mbuf sbuf tbl; do\
		    < deps.$$dir sed -e "s/^src/$$dir/;s/^[a-z-]*\.o:/$$dir\/&/" >> dependencies;\
		    $(RM) deps.$$dir;\
		  done;\
		fi

include ../maketail
