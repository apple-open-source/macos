# $Id: Makefile.in,v 1.52 2007/04/28 14:56:11 tom Exp $
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
# Makefile for panels source code.
#
# This makes the following:
#	libraries (normal/debug/profile/shared)
#
# The variable 'srcdir' refers to the source-distribution, and can be set with
# the configure script by "--srcdir=DIR".
#
# The rules are organized to produce the libraries for the configured models,

# turn off _all_ suffix rules; we'll generate our own
.SUFFIXES:

SHELL		= /bin/sh
THIS		= Makefile

x		= 
o		= .o

MODEL		= shared
DESTDIR		= 
srcdir		= /tmp/ncurses-27.roots/ncurses-27/ncurses/panel
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
INSTALL_LIB	= /usr/bin/install -c -m 644
INSTALL_PROG	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

AR		= ar
AR_OPTS		= rv
AWK		= awk
LD		= ld
LN_S		= ln -s

CC		= gcc
CPP		= gcc -E
CFLAGS		= -arch i386 -arch x86_64 -arch ppc -g -Os -pipe -isysroot /  --param max-inline-insns-single=1200  -no-cpp-precomp

CPPFLAGS	= -I/tmp/ncurses-27.roots/ncurses-27/ncurses/ncurses -DHAVE_CONFIG_H -I${srcdir}/../include -I. -I../include  -U_XOPEN_SOURCE -D_XOPEN_SOURCE=500 -DSIGWINCH=28 -D_XOPEN_SOURCE_EXTENDED -DNDEBUG

CCFLAGS		= $(CPPFLAGS) $(CFLAGS)

CFLAGS_LIBTOOL	= $(CCFLAGS)
CFLAGS_NORMAL	= $(CCFLAGS)
CFLAGS_DEBUG	= $(CCFLAGS) -g -DTRACE
CFLAGS_PROFILE	= $(CCFLAGS) -pg
CFLAGS_SHARED	= $(CCFLAGS) -dynamic

CFLAGS_DEFAULT	= $(CFLAGS_SHARED)

LINK		= $(LIBTOOL_LINK)
LDFLAGS		= -arch i386 -arch x86_64 -arch ppc             -Wl,-search_paths_first  

SHLIB_DIRS	= -L../lib
SHLIB_LIST	= $(SHLIB_DIRS) -lncurses 

MK_SHARED_LIB	= ${CC} ${CFLAGS} -dynamiclib -install_name ${libdir}/`basename $@` -compatibility_version ${ABI_VERSION} -current_version ${ABI_VERSION} -o $@

NCURSES_MAJOR	= 5
NCURSES_MINOR	= 7
REL_VERSION	= 5.7
ABI_VERSION	= 5.4

RANLIB		= ranlib

LIBRARIES	=  ../lib/libpanel.${ABI_VERSION}.dylib

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lpanel -lncurses 

AUTO_SRC	= \
		../include/panel.h

################################################################################
all \
libs \
install ::	$(AUTO_SRC) $(LIBRARIES)

sources:	$(AUTO_SRC)

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) :
	sh $(srcdir)/../mkdirs.sh $@

# make a copy to simplify include-paths while still keeping panel's include
# file in this directory.
../include/panel.h : $(srcdir)/panel.h
	-rm -f $@
	cp $(srcdir)/panel.h $@

PANEL_PRIV_H = \
	$(srcdir)/panel.priv.h \
	$(srcdir)/panel.h

tags:
	ctags *.[ch]

#TAGS:
#	etags *.[ch]

mostlyclean ::
	-rm -f core tags TAGS *~ *.bak *.i *.ln *.atac trace

clean :: mostlyclean
	-rm -f $(AUTO_SRC)

distclean :: clean
	-rm -f Makefile

realclean :: distclean

###############################################################################
# The remainder of this file is automatically generated during configuration
###############################################################################

# generated by mk-0th.awk
#   libname:    panel
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(srcdir)/panel.c \
	$(srcdir)/p_above.c \
	$(srcdir)/p_below.c \
	$(srcdir)/p_bottom.c \
	$(srcdir)/p_delete.c \
	$(srcdir)/p_hide.c \
	$(srcdir)/p_hidden.c \
	$(srcdir)/p_move.c \
	$(srcdir)/p_new.c \
	$(srcdir)/p_replace.c \
	$(srcdir)/p_show.c \
	$(srcdir)/p_top.c \
	$(srcdir)/p_update.c \
	$(srcdir)/p_user.c \
	$(srcdir)/p_win.c

# Producing llib-lpanel is time-consuming, so there's no direct-dependency for
# it in the lintlib rule.  We'll only remove in the cleanest setup.
clean ::
	rm -f llib-lpanel.*

realclean ::
	rm -f llib-lpanel

llib-lpanel : $(C_SRC)
	cproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) $(C_SRC) >$@

lintlib :
	sh $(srcdir)/../misc/makellib panel $(CPPFLAGS)
lint :
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(C_SRC) $(LINT_LIBS)

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          panel
#  traces:        DEBUG
#  MODEL:         SHARED
#  model:         obj_s
#  prefix:        lib
#  suffix:        .dylib
#  subset:        ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#  ShlibVer:      abi
#  ShlibVerInfix: yes
#  TermlibRoot:   ncurses
#  TermlibSuffix: .dylib
#  ReLink:        yes
#  DoLinks:       yes
#  rmSoLocs:      no
#  ldconfig:      
#  overwrite:     yes
#  depend:        ../include/ncurses_cfg.h ${srcdir}/panel.priv.h
#  host:          i386-apple-darwin10.0

SHARED_OBJS = \
	../obj_s/panel$o \
	../obj_s/p_above$o \
	../obj_s/p_below$o \
	../obj_s/p_bottom$o \
	../obj_s/p_delete$o \
	../obj_s/p_hide$o \
	../obj_s/p_hidden$o \
	../obj_s/p_move$o \
	../obj_s/p_new$o \
	../obj_s/p_replace$o \
	../obj_s/p_show$o \
	../obj_s/p_top$o \
	../obj_s/p_update$o \
	../obj_s/p_user$o \
	../obj_s/p_win$o

$(SHARED_OBJS) : ../include/ncurses_cfg.h ${srcdir}/panel.priv.h

../lib/libpanel.$(ABI_VERSION).dylib : \
		../lib \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd ../lib && (rm -f libpanel.dylib; $(LN_S) libpanel.$(ABI_VERSION).dylib libpanel.dylib; )

install \
install.libs \
install.panel :: $(DESTDIR)$(libdir)/libpanel.$(ABI_VERSION).dylib

$(DESTDIR)$(libdir)/libpanel.$(ABI_VERSION).dylib : \
		$(DESTDIR)$(libdir) \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd $(DESTDIR)$(libdir) && (rm -f libpanel.dylib; $(LN_S) libpanel.$(ABI_VERSION).dylib libpanel.dylib; )

uninstall \
uninstall.libs \
uninstall.panel ::
	@echo uninstalling $(DESTDIR)$(libdir)/libpanel.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libpanel.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libpanel.dylib

clean ::
	-rm -f ../lib/libpanel.$(ABI_VERSION).dylib
	-rm -f ../lib/libpanel.dylib

mostlyclean::
	-rm -f $(SHARED_OBJS)

# generated by mk-2nd.awk
#   model:      obj_s
#   MODEL:      SHARED
#   echo:       yes
#   subset:     ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#   crenames:   yes
#   cxxrenames: 
#   traces:     DEBUG
#   srcdir:     /tmp/ncurses-27.roots/ncurses-27/ncurses

../obj_s/panel$o :	$(srcdir)/panel.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/panel.c -o ../obj_s/panel$o

../obj_s/p_above$o :	$(srcdir)/p_above.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_above.c -o ../obj_s/p_above$o

../obj_s/p_below$o :	$(srcdir)/p_below.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_below.c -o ../obj_s/p_below$o

../obj_s/p_bottom$o :	$(srcdir)/p_bottom.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_bottom.c -o ../obj_s/p_bottom$o

../obj_s/p_delete$o :	$(srcdir)/p_delete.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_delete.c -o ../obj_s/p_delete$o

../obj_s/p_hide$o :	$(srcdir)/p_hide.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_hide.c -o ../obj_s/p_hide$o

../obj_s/p_hidden$o :	$(srcdir)/p_hidden.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_hidden.c -o ../obj_s/p_hidden$o

../obj_s/p_move$o :	$(srcdir)/p_move.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_move.c -o ../obj_s/p_move$o

../obj_s/p_new$o :	$(srcdir)/p_new.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_new.c -o ../obj_s/p_new$o

../obj_s/p_replace$o :	$(srcdir)/p_replace.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_replace.c -o ../obj_s/p_replace$o

../obj_s/p_show$o :	$(srcdir)/p_show.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_show.c -o ../obj_s/p_show$o

../obj_s/p_top$o :	$(srcdir)/p_top.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_top.c -o ../obj_s/p_top$o

../obj_s/p_update$o :	$(srcdir)/p_update.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_update.c -o ../obj_s/p_update$o

../obj_s/p_user$o :	$(srcdir)/p_user.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_user.c -o ../obj_s/p_user$o

../obj_s/p_win$o :	$(srcdir)/p_win.c \
			$(PANEL_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/p_win.c -o ../obj_s/p_win$o


# generated by mk-hdr.awk
#  subset:     ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#  compat:     yes

${DESTDIR}${includedir} :
	sh ${srcdir}/../mkdirs.sh $@

install \
install.libs \
install.includes :: ${AUTO_SRC} ${DESTDIR}${includedir} \
		$(srcdir)/panel.h
	@ (cd ${DESTDIR}${includedir} && rm -f panel.h) ; ../headers.sh ${INSTALL_DATA} ${DESTDIR}${includedir} ${srcdir} $(srcdir)/panel.h

uninstall \
uninstall.libs \
uninstall.includes ::
	-@ (cd ${DESTDIR}${includedir} && rm -f panel.h)
depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
