# $Id: Makefile.in,v 1.79 2008/09/07 13:58:55 tom Exp $
##############################################################################
# Copyright (c) 1998-2006,2007 Free Software Foundation, Inc.                #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
#
# Author: Thomas E. Dickey 1996-on
#
# Makefile for ncurses source code.
#
# This makes the ncurses utility programs.
#
# The variable 'srcdir' refers to the source-distribution, and can be set with
# the configure script by "--srcdir=DIR".
#
# The rules are organized to produce the libraries for the configured models,
# and the programs with the configured default model.

# turn off _all_ suffix rules; we'll generate our own
.SUFFIXES:

SHELL		= /bin/sh
THIS		= Makefile

CF_MFLAGS 	= 


x		= 
o		= .o

MODEL		= ../objects
DESTDIR		= 
top_srcdir	= ..
srcdir		= .
prefix		= /usr
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
includedir	= ${prefix}/include
datadir		= ${prefix}/share

LIBTOOL		= 
LIBTOOL_CLEAN	= 
LIBTOOL_COMPILE	= 
LIBTOOL_LINK	= ${CC}
LIBTOOL_INSTALL	= 
LIBTOOL_UNINSTALL = 

INSTALL		= /usr/bin/install -c
INSTALL_PROG	= ${INSTALL}
transform	= s,x,x,

AWK		= awk
LN_S		= ln -s

CC		= gcc
CPP		= gcc -E
CFLAGS		= -O2 --param max-inline-insns-single=1200  -no-cpp-precomp -mdynamic-no-pic

INCDIR		= $(top_srcdir)/include
CPPFLAGS	= -I../progs -I$(srcdir) -DHAVE_CONFIG_H -I. -I../include  -U_XOPEN_SOURCE -D_XOPEN_SOURCE=500 -DSIGWINCH=28 -DNDEBUG

CCFLAGS		= $(CPPFLAGS) $(CFLAGS)

CFLAGS_LIBTOOL	= $(CCFLAGS)
CFLAGS_NORMAL	= $(CCFLAGS)
CFLAGS_DEBUG	= $(CCFLAGS) -g -DTRACE
CFLAGS_PROFILE	= $(CCFLAGS) -pg
CFLAGS_SHARED	= $(CCFLAGS) -dynamic

CFLAGS_DEFAULT	= $(CFLAGS_NORMAL)

REL_VERSION	= 5.7
ABI_VERSION	= 5
LOCAL_LIBDIR	= /Volumes/data/stripes/R/PR-6917817/ncurses/ncurses/lib

LD		= ld
LINK		=  $(LIBTOOL_LINK)
LDFLAGS		=   -Wl,-search_paths_first

LDFLAGS_LIBTOOL	= $(LDFLAGS) $(CFLAGS_LIBTOOL)
LDFLAGS_NORMAL	= $(LDFLAGS) $(CFLAGS_NORMAL)
LDFLAGS_DEBUG	= $(LDFLAGS) $(CFLAGS_DEBUG)
LDFLAGS_PROFILE	= $(LDFLAGS) $(CFLAGS_PROFILE) 
LDFLAGS_SHARED	= $(LDFLAGS) $(CFLAGS_SHARED) 

LDFLAGS_DEFAULT	= $(LDFLAGS_NORMAL)

LIBS_TIC	= -static -L../lib -lncurses -L../lib -lncurses -dynamic  
LDFLAGS_TIC	= $(LDFLAGS_NORMAL) $(LIBS_TIC)

LIBS_TINFO	= -static -L../lib -lncurses -dynamic  
LDFLAGS_TINFO	= $(LDFLAGS_NORMAL) $(LIBS_TINFO)

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lncurses 

AUTO_SRC = \
	termsort.c \
	transform.h

# tic relies on direct access to the terminfo database
GET_PROGS = infocmp$x clear$x tput$x tset$x toe$x
PUT_PROGS =  tic$x
PROGS = $(PUT_PROGS) $(GET_PROGS)

# Default library, for linking applications
DEPS_CURSES = ../lib/libncurses.a

HEADER_DEPS	= \
	../include/curses.h \
	$(INCDIR)/term_entry.h \
	$(INCDIR)/tic.h \
	$(INCDIR)/nc_alloc.h

################################################################################
all:		$(AUTO_SRC) $(PROGS)

sources:	$(AUTO_SRC)

install: 	$(AUTO_SRC) install.progs
uninstall: uninstall.progs

# this line simplifies the configure-script
libs \
install.libs \
uninstall.libs:

TRANSFORM = sed 's/$x$$//'|sed '$(transform)'|sed 's/$$/$x/'

# transformed names for installing files
actual_captoinfo = `echo captoinfo$x| $(TRANSFORM)`
actual_clear     = `echo clear$x|     $(TRANSFORM)`
actual_infocmp   = `echo infocmp$x|   $(TRANSFORM)`
actual_infotocap = `echo infotocap$x| $(TRANSFORM)`
actual_init      = `echo init$x|      $(TRANSFORM)`
actual_reset     = `echo reset$x|     $(TRANSFORM)`
actual_tic       = `echo tic$x|       $(TRANSFORM)`
actual_toe       = `echo toe$x|       $(TRANSFORM)`
actual_tput      = `echo tput$x|      $(TRANSFORM)`
actual_tset      = `echo tset$x|      $(TRANSFORM)`

# transformed names for comparing at runtime
define_captoinfo = `echo captoinfo|   $(TRANSFORM)`
define_infotocap = `echo infotocap|   $(TRANSFORM)`
define_init      = `echo init|        $(TRANSFORM)`
define_reset     = `echo reset|       $(TRANSFORM)`

transform.h :
	echo "#define PROG_CAPTOINFO \"$(define_captoinfo)\"" >$@
	echo "#define PROG_INFOTOCAP \"$(define_infotocap)\"" >>$@
	echo "#define PROG_RESET     \"$(define_reset)\""     >>$@
	echo "#define PROG_INIT      \"$(define_init)\""      >>$@

install.progs: $(AUTO_SRC) $(PROGS) $(DESTDIR)$(bindir)
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) tic$x     $(DESTDIR)$(bindir)/$(actual_tic)
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) toe$x     $(DESTDIR)$(bindir)/$(actual_toe)
	@echo "linking $(actual_infotocap) to $(actual_tic)"
	-@rm -f $(DESTDIR)$(bindir)/$(actual_infotocap)
	(cd $(DESTDIR)$(bindir) && $(LN_S) $(actual_tic) $(actual_infotocap))
	@echo "linking $(actual_captoinfo) to $(actual_tic)"
	-@rm -f $(DESTDIR)$(bindir)/$(actual_captoinfo)
	(cd $(DESTDIR)$(bindir) && $(LN_S) $(actual_tic) $(actual_captoinfo))
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) infocmp$x $(DESTDIR)$(bindir)/$(actual_infocmp)
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) clear$x   $(DESTDIR)$(bindir)/$(actual_clear)
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) tput$x    $(DESTDIR)$(bindir)/$(actual_tput)
	$(LIBTOOL_INSTALL) $(INSTALL_PROG) tset$x    $(DESTDIR)$(bindir)/$(actual_tset)
	@echo "linking $(actual_reset) to $(actual_tset)"
	-@rm -f $(DESTDIR)$(bindir)/$(actual_reset)
	(cd $(DESTDIR)$(bindir) && $(LN_S) $(actual_tset) $(actual_reset))

uninstall.progs:
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_tic)
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_toe)
	-@rm -f $(DESTDIR)$(bindir)/$(actual_captoinfo)
	-@rm -f $(DESTDIR)$(bindir)/$(actual_infotocap)
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_infocmp)
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_clear)
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_tput)
	-@$(LIBTOOL_CLEAN) rm -f $(DESTDIR)$(bindir)/$(actual_tset)
	-@rm -f $(DESTDIR)$(bindir)/$(actual_reset)

$(DESTDIR)$(bindir) :
	sh $(srcdir)/../mkdirs.sh $@

#
# Utilities normally built by make all start here
#

DEPS_TIC = \
	$(MODEL)/tic$o \
	$(MODEL)/dump_entry$o

tic$x: $(DEPS_TIC) $(DEPS_CURSES) transform.h
	 $(LINK) $(DEPS_TIC) $(LDFLAGS_TIC) -o $@

DEPS_TOE = \
	$(MODEL)/toe$o

toe$x: $(DEPS_TOE) $(DEPS_CURSES)
	 $(LINK) $(DEPS_TOE) $(LDFLAGS_TIC) -o $@

DEPS_CLEAR = \
	$(MODEL)/clear$o

clear$x: $(DEPS_CLEAR) $(DEPS_CURSES)
	 $(LINK) $(DEPS_CLEAR) $(LDFLAGS_TINFO) -o $@

DEPS_TPUT = \
	$(MODEL)/tput$o

tput$x: $(DEPS_TPUT) $(DEPS_CURSES) transform.h
	 $(LINK) $(DEPS_TPUT) $(LDFLAGS_TINFO) -o $@

DEPS_INFOCMP = \
	$(MODEL)/infocmp$o \
	$(MODEL)/dump_entry$o

infocmp$x: $(DEPS_INFOCMP) $(DEPS_CURSES)
	 $(LINK) $(DEPS_INFOCMP) $(LDFLAGS_TIC) -o $@

DEPS_TSET = \
	$(MODEL)/tset$o

tset$x: $(DEPS_TSET) $(DEPS_CURSES) transform.h
	 $(LINK) $(DEPS_TSET) $(LDFLAGS_TINFO) -o $@

termsort.c: $(srcdir)/MKtermsort.sh
	sh $(srcdir)/MKtermsort.sh $(AWK) $(srcdir)/../include/Caps >$@

#
# Utility productions start here
#

tags:
	ctags *.[ch]

#TAGS:
#	etags *.[ch]

mostlyclean ::
	-rm -f core tags TAGS *~ *.bak *.i *.ln *.atac trace

clean :: mostlyclean
	-sh -c "if test -n '$x' ; then $(MAKE) clean x=''; fi"
	-rm -f $(AUTO_SRC)
	-rm -f $(PROGS)
	-rm -rf .libs

distclean :: clean
	-rm -f Makefile

realclean :: distclean

# These rules are used to allow "make -n" to work on a clean directory-tree
../include/hashsize.h \
../include/parametrized.h \
../include/term.h :
	cd ../include; $(MAKE) $(CF_MFLAGS)
$(DEPS_CURSES) :
	cd ../ncurses; $(MAKE) $(CF_MFLAGS)

lint:
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/tic.c     $(srcdir)/dump_entry.c $(LINT_LIBS)
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/toe.c     $(srcdir)/dump_entry.c $(LINT_LIBS)
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/clear.c                          $(LINT_LIBS)
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/infocmp.c $(srcdir)/dump_entry.c $(LINT_LIBS)
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/tput.c                           $(LINT_LIBS)
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/tset.c    $(srcdir)/dump_entry.c $(LINT_LIBS)

###############################################################################
# The remainder of this file is automatically generated during configuration
###############################################################################

# generated by mk-0th.awk
#   libname:    progs
#   subsets:    ticlib+termlib+ext_tinfo+base+ext_funcs

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(srcdir)/clear.c \
	$(srcdir)/tic.c \
	$(srcdir)/toe.c \
	$(srcdir)/dump_entry.c \
	$(srcdir)/infocmp.c \
	$(srcdir)/tput.c \
	$(srcdir)/tset.c

lintlib :
	@echo no action needed

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+ext_funcs)
#  name:          progs
#  traces:        DEBUG
#  MODEL:         NORMAL
#  model:         objects
#  prefix:        lib
#  suffix:        .a
#  subset:        ticlib+termlib+ext_tinfo+base+ext_funcs
#  ShlibVer:      abi
#  ShlibVerInfix: yes
#  TermlibRoot:   ncurses
#  TermlibSuffix: .a
#  ReLink:        
#  DoLinks:       yes
#  rmSoLocs:      no
#  ldconfig:      
#  overwrite:     yes
#  depend:        ../include/ncurses_cfg.h ./progs.priv.h
#  host:          i386-apple-darwin10.0.0

NORMAL_OBJS = \
	../objects/clear$o \
	../objects/tic$o \
	../objects/toe$o \
	../objects/dump_entry$o \
	../objects/infocmp$o \
	../objects/tput$o \
	../objects/tset$o

$(NORMAL_OBJS) : ../include/ncurses_cfg.h ./progs.priv.h

mostlyclean::
	-rm -f $(NORMAL_OBJS)

clean ::
	-rm -f $(NORMAL_OBJS)

# generated by mk-2nd.awk
#   model:      objects
#   MODEL:      NORMAL
#   echo:       yes
#   subset:     ticlib+termlib+ext_tinfo+base+ext_funcs
#   crenames:   yes
#   cxxrenames: yes
#   traces:     DEBUG
#   srcdir:     .

../objects/clear$o :	$(srcdir)/clear.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/clear.c -o ../objects/clear$o

../objects/tic$o :	$(srcdir)/tic.c \
			$(HEADER_DEPS) \
			transform.h \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/tic.c -o ../objects/tic$o

../objects/toe$o :	$(srcdir)/toe.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/toe.c -o ../objects/toe$o

../objects/dump_entry$o :	$(srcdir)/dump_entry.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h \
			../include/parametrized.h \
			$(INCDIR)/capdefaults.c \
			termsort.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/dump_entry.c -o ../objects/dump_entry$o

../objects/infocmp$o :	$(srcdir)/infocmp.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/infocmp.c -o ../objects/infocmp$o

../objects/tput$o :	$(srcdir)/tput.c \
			$(HEADER_DEPS) \
			transform.h \
			termsort.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/tput.c -o ../objects/tput$o

../objects/tset$o :	$(srcdir)/tset.c \
			$(HEADER_DEPS) \
			transform.h \
			$(srcdir)/dump_entry.h \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_NORMAL) -c ../progs/tset.c -o ../objects/tset$o


# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+ext_funcs)
#  name:          progs
#  traces:        DEBUG
#  MODEL:         DEBUG
#  model:         obj_g
#  prefix:        lib
#  suffix:        _g.a
#  subset:        ticlib+termlib+ext_tinfo+base+ext_funcs
#  ShlibVer:      abi
#  ShlibVerInfix: yes
#  TermlibRoot:   ncurses
#  TermlibSuffix: .a
#  ReLink:        
#  DoLinks:       yes
#  rmSoLocs:      no
#  ldconfig:      
#  overwrite:     yes
#  depend:        ../include/ncurses_cfg.h ./progs.priv.h
#  host:          i386-apple-darwin10.0.0

DEBUG_OBJS = \
	../obj_g/clear$o \
	../obj_g/tic$o \
	../obj_g/toe$o \
	../obj_g/dump_entry$o \
	../obj_g/infocmp$o \
	../obj_g/tput$o \
	../obj_g/tset$o

$(DEBUG_OBJS) : ../include/ncurses_cfg.h ./progs.priv.h

mostlyclean::
	-rm -f $(DEBUG_OBJS)

clean ::
	-rm -f $(DEBUG_OBJS)

# generated by mk-2nd.awk
#   model:      obj_g
#   MODEL:      DEBUG
#   echo:       yes
#   subset:     ticlib+termlib+ext_tinfo+base+ext_funcs
#   crenames:   yes
#   cxxrenames: yes
#   traces:     DEBUG
#   srcdir:     .

../obj_g/clear$o :	$(srcdir)/clear.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/clear.c -o ../obj_g/clear$o

../obj_g/tic$o :	$(srcdir)/tic.c \
			$(HEADER_DEPS) \
			transform.h \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/tic.c -o ../obj_g/tic$o

../obj_g/toe$o :	$(srcdir)/toe.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/toe.c -o ../obj_g/toe$o

../obj_g/dump_entry$o :	$(srcdir)/dump_entry.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h \
			../include/parametrized.h \
			$(INCDIR)/capdefaults.c \
			termsort.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/dump_entry.c -o ../obj_g/dump_entry$o

../obj_g/infocmp$o :	$(srcdir)/infocmp.c \
			$(HEADER_DEPS) \
			$(srcdir)/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/infocmp.c -o ../obj_g/infocmp$o

../obj_g/tput$o :	$(srcdir)/tput.c \
			$(HEADER_DEPS) \
			transform.h \
			termsort.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/tput.c -o ../obj_g/tput$o

../obj_g/tset$o :	$(srcdir)/tset.c \
			$(HEADER_DEPS) \
			transform.h \
			$(srcdir)/dump_entry.h \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_DEBUG) -c ../progs/tset.c -o ../obj_g/tset$o

depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
