# makefile (top level)
#
#  Builds and installs the snacc ASN.1 compiler and libraries.
#
#  MS 92
#
# $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/makefile,v 1.1.1.1 2001/05/18 23:14:04 mb Exp $
# $Log: makefile,v $
# Revision 1.1.1.1  2001/05/18 23:14:04  mb
# Move from private repository to open source repository
#
# Revision 1.3  2000/12/22 00:00:40  dmitch
# Add  include makehead.static due to apparently broken autoconf, which now generates empty makehead file.
#
# Revision 1.2  2000/06/15 18:49:46  dmitch
# Trimmed EXAMPLESDIR to comply with reality.
#
# Revision 1.1.1.1  1999/03/16 18:05:49  aram
# Originals from SMIME Free Library.
#
# Revision 1.15  1997/08/27 15:53:11  wan
# Added generic table decoding, debug routines, berdecode, and asnwish.
#
# Revision 1.14  1997/03/03 11:58:26  wan
# Final pre-delivery stuff (I hope).
#
# Revision 1.13  1997/02/16 19:55:22  rj
# remove tcl-p when making clobber
#
# Revision 1.12  1997/02/16 18:30:33  rj
# correct path to configure script
#
# Revision 1.11  1997/02/16 16:52:05  rj
# generate tcl-p before it is needed...
#
# Revision 1.10  1997/02/15 20:33:33  rj
# trick to make making clean/clobber working with gmake: set MAKE_CLEAN_HACK (in makefile), and don't include the dependencies files (in maketail) if it is set.
#
# Revision 1.9  1995/07/26 21:16:56  rj
# distribute tcl-p.c
#
# Revision 1.8  1995/07/24  14:54:22  rj
# type table tools integrated.
#
# tiny .../tcl-p utility program that returns 0 if snacc's tcl interface is to be made.
#
# dummy action added to config.h.in and config.h rules to trick many makes' overoptimization.
#
# Revision 1.7  1995/02/22  09:49:07  rj
# turned the phony target `pack-tar' into a true target: $(VDIR).tar.gz.
#
# Revision 1.6  1995/02/20  11:21:27  rj
# use install-sh instead of install.sh.
# phony target distclean: don't remove the distributed file configure and stamp-h.in (they cannot be remade everywhere).
#
# Revision 1.5  1995/02/17  14:26:39  rj
# adjustments for autoconf 2.x
#
# Revision 1.4  1994/10/08  04:42:56  rj
# config.h.bot removed.
# snacc.h and policy.h added.
#
# Revision 1.3  1994/09/01  01:34:34  rj
# gen-distfiles phony target to generate up-to-date versions of the distfiles.
#
# Revision 1.2  1994/08/31  23:19:22  rj
# a lot of enhancements:
# - examples integrated, with some phony targets.
# - autoconf stuff gets automatically remade.
# - dependency boot strapping (some makes need it).
# - distributions via file list, therefore no need to clean the tree and to remove private files.
#
# Revision 1.1  1994/08/28  09:20:17  rj
# first check-in. for a list of changes to 1.1 please refer to the ChangeLog.

TOP		= .

include makehead
include makehead.static

SUBDIRS		= compiler c-lib c++-lib tbl-tools asn1specs doc
XDIRS		= tcl-lib tcl-asn

# Apple change: the tbl-example doesn't run properly; tcl-example doesn't build Skip them.
EXAMPLESDIRS	= c-examples c++-examples
#EXAMPLESDIRS	= c-examples c++-examples tbl-example tcl-example

DISTFILES = \
	COPYING						\
	README NEWS INSTALL				\
	ChangeLog version.h				\
	configure.in configure				\
	acconfig.h config.h.in stamp-h.in		\
	policy.h snacc.h				\
	tcl-p.c						\
	makefile makehead.in maketail			\
	mkinstalldirs install-sh

#-------------------------------------------------------------------------------

.PHONY:	default all subdirs $(SUBDIRS) examples $(EXAMPLESDIRS) $(XDIRS) depend c c++ check c-check c++-check install c-install c++-install clean clobber tar

default::	compiler c-lib c++-lib tbl-tools tcl-asn

tcl-p:		$(TCL-P)
$(TCL-P):	tcl-p.c snacc.h config.h policy.h
		$(REASON)
		$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

depend $(SUBDIRS) $(EXAMPLESDIRS) $(XDIRS)::	config.h version.h

# the following hack is needed for older make versions
# (gmake doesn't need it):
depend::
		@for dir in $(SUBDIRS) tbl-example; do\
		  test -f $$dir/dependencies || touch $$dir/dependencies;\
		done
		@for dir in c-examples c++-examples; do\
		  (cd $$dir && $(MAKE) init-depend);\
		done

depend install gen-distfiles::
		$(MAKE) subdirs examples xdirs subtarget=$@

clean clobber::
		$(MAKE) subdirs examples xdirs subtarget=$@ MAKE_CLEAN_HACK=xyzzy

subdirs::	$(SUBDIRS)
examples::	$(EXAMPLESDIRS)
xdirs::		$(XDIRS)
$(SUBDIRS)	\
$(EXAMPLESDIRS)::
		cd $@ && $(MAKE) $(subtarget)
tcl-lib::
		$(MAKE) $(TCL-P)
		if $(TCL-P); then cd $@ && $(MAKE) $(subtarget); fi

tcl-asn::
		$(MAKE) $(TCL-P)
		if $(TCL-P); then cd $@ && $(MAKE) $(subtarget); fi

# compiler, C runtime libraries and type table tools:
c::		compiler c-lib tbl-tools tcl-asn
c-check::	c c-lib-check tbl-check
c-lib-check::
		$(MAKE) c-examples subtarget=check
tbl-check::
		$(MAKE) tbl-example subtarget=check
c-install::
		$(MAKE) c asn1specs subtarget=install

# build compiler and C++ runtime library:
c++::		compiler c++-lib tcl-lib
c++-check::	c++
c++-check::
		$(MAKE) c++-examples subtarget=check
c++-install::
		$(MAKE) c++ asn1specs subtarget=install

# create compiler, build C and C++ runtime lib archive, type table tools, run a few checks:
all::		compiler c-lib c++-lib tbl-tools tcl-asn check

check::		c-check c++-check

$(srcdir)/configure:	configure.in
		$(REASON)
		cd $(srcdir) && autoconf

# autoheader might not change config.h.in, so touch a stamp file
$(srcdir)/config.h.in:	stamp-h.in
		@true
$(srcdir)/stamp-h.in:	configure.in acconfig.h
		$(REASON)
		cd ${srcdir} && autoheader
		date > $@

# config.status might not change config.h
config.h:	stamp-h
		@true
stamp-h:	config.h.in config.status
		$(REASON)
		CONFIG_FILES= CONFIG_HEADERS=config.h ./config.status

makehead:	makehead.in config.status
		$(REASON)
		CONFIG_FILES=makehead CONFIG_HEADERS= ./config.status

config.status:	$(srcdir)/configure
		$(REASON)
		./config.status --recheck

clobber distclean::
		$(RM) config.log config.cache config.status config.h stamp-h
		$(RM) $(TCL-P)

gen-distfiles::	$(DISTFILES)

distfiles::
		echo "$(DISTFILES)";\
		for dir in $(SUBDIRS) $(EXAMPLESDIRS) $(XDIRS); do\
		  subfiles=`cd $$dir && $(MAKE) -s $@`;\
		  for file in $$subfiles; do\
		    echo "$$dir/$$file";\
		  done;\
		done

tar::		gen-distfiles

tar::
		vdir=snacc-`sed -e '/VERSION/!d' -e 's/.*"\(.*\)".*/\1/' -e q version.h`;\
		distfiles=`$(MAKE) -s distfiles ECHO=:`;\
		for file in $$distfiles; do\
		  files="$$files $$vdir/$$file";\
		done;\
		[ -d $$vdir ] || ln -s . $$vdir;\
		$(RM) $$vdir.tar.gz;\
		gtar zcvvhf $$vdir.tar.gz $$files;\
		$(RM) $$vdir;\
		ls -l $$vdir.tar.gz

# this one is for internal use only:
$(VDIR).tar.gz:	$(FILES)
		$(REASON)
		$(RM) $@
		gtar zcvvhf $@ $(FILES)
