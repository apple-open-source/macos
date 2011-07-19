# $Id: Makefile.in,v 1.48 2007/04/28 14:56:11 tom Exp $
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
# Makefile for menu source code.
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
srcdir		= /tmp/ncurses-27.roots/ncurses-27/ncurses/menu
prefix		= /usr
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
includedir	= ${prefix}/include

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

LIBRARIES	=  ../lib/libmenu.${ABI_VERSION}.dylib

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lmenu -lncurses 

AUTO_SRC	= \
		../include/menu.h \
		../include/eti.h \
		../include/mf_common.h

################################################################################
all \
libs \
install ::	$(AUTO_SRC) $(LIBRARIES)

sources :	$(AUTO_SRC)

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) :
	sh $(srcdir)/../mkdirs.sh $@

# make copies to simplify include-paths while still keeping menu's include
# file in this directory.
../include/menu.h : $(srcdir)/menu.h
	-rm -f $@
	cp $(srcdir)/menu.h $@
../include/eti.h : $(srcdir)/eti.h
	-rm -f $@
	cp $(srcdir)/eti.h $@
../include/mf_common.h : $(srcdir)/mf_common.h
	-rm -f $@
	cp $(srcdir)/mf_common.h $@

MENU_PRIV_H = \
	$(srcdir)/menu.priv.h \
	$(AUTO_SRC) \
	../include/curses.h

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
#   libname:    menu
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(srcdir)/m_attribs.c \
	$(srcdir)/m_cursor.c \
	$(srcdir)/m_driver.c \
	$(srcdir)/m_format.c \
	$(srcdir)/m_global.c \
	$(srcdir)/m_hook.c \
	$(srcdir)/m_item_cur.c \
	$(srcdir)/m_item_nam.c \
	$(srcdir)/m_item_new.c \
	$(srcdir)/m_item_opt.c \
	$(srcdir)/m_item_top.c \
	$(srcdir)/m_item_use.c \
	$(srcdir)/m_item_val.c \
	$(srcdir)/m_item_vis.c \
	$(srcdir)/m_items.c \
	$(srcdir)/m_new.c \
	$(srcdir)/m_opts.c \
	$(srcdir)/m_pad.c \
	$(srcdir)/m_pattern.c \
	$(srcdir)/m_post.c \
	$(srcdir)/m_req_name.c \
	$(srcdir)/m_scale.c \
	$(srcdir)/m_spacing.c \
	$(srcdir)/m_sub.c \
	$(srcdir)/m_trace.c \
	$(srcdir)/m_userptr.c \
	$(srcdir)/m_win.c

# Producing llib-lmenu is time-consuming, so there's no direct-dependency for
# it in the lintlib rule.  We'll only remove in the cleanest setup.
clean ::
	rm -f llib-lmenu.*

realclean ::
	rm -f llib-lmenu

llib-lmenu : $(C_SRC)
	cproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) $(C_SRC) >$@

lintlib :
	sh $(srcdir)/../misc/makellib menu $(CPPFLAGS)
lint :
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(C_SRC) $(LINT_LIBS)

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          menu
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
#  depend:        ../include/ncurses_cfg.h ${srcdir}/menu.priv.h
#  host:          i386-apple-darwin10.0

SHARED_OBJS = \
	../obj_s/m_attribs$o \
	../obj_s/m_cursor$o \
	../obj_s/m_driver$o \
	../obj_s/m_format$o \
	../obj_s/m_global$o \
	../obj_s/m_hook$o \
	../obj_s/m_item_cur$o \
	../obj_s/m_item_nam$o \
	../obj_s/m_item_new$o \
	../obj_s/m_item_opt$o \
	../obj_s/m_item_top$o \
	../obj_s/m_item_use$o \
	../obj_s/m_item_val$o \
	../obj_s/m_item_vis$o \
	../obj_s/m_items$o \
	../obj_s/m_new$o \
	../obj_s/m_opts$o \
	../obj_s/m_pad$o \
	../obj_s/m_pattern$o \
	../obj_s/m_post$o \
	../obj_s/m_req_name$o \
	../obj_s/m_scale$o \
	../obj_s/m_spacing$o \
	../obj_s/m_sub$o \
	../obj_s/m_userptr$o \
	../obj_s/m_win$o

$(SHARED_OBJS) : ../include/ncurses_cfg.h ${srcdir}/menu.priv.h

../lib/libmenu.$(ABI_VERSION).dylib : \
		../lib \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd ../lib && (rm -f libmenu.dylib; $(LN_S) libmenu.$(ABI_VERSION).dylib libmenu.dylib; )

install \
install.libs \
install.menu :: $(DESTDIR)$(libdir)/libmenu.$(ABI_VERSION).dylib

$(DESTDIR)$(libdir)/libmenu.$(ABI_VERSION).dylib : \
		$(DESTDIR)$(libdir) \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd $(DESTDIR)$(libdir) && (rm -f libmenu.dylib; $(LN_S) libmenu.$(ABI_VERSION).dylib libmenu.dylib; )

uninstall \
uninstall.libs \
uninstall.menu ::
	@echo uninstalling $(DESTDIR)$(libdir)/libmenu.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libmenu.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libmenu.dylib

clean ::
	-rm -f ../lib/libmenu.$(ABI_VERSION).dylib
	-rm -f ../lib/libmenu.dylib

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

../obj_s/m_attribs$o :	$(srcdir)/m_attribs.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_attribs.c -o ../obj_s/m_attribs$o

../obj_s/m_cursor$o :	$(srcdir)/m_cursor.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_cursor.c -o ../obj_s/m_cursor$o

../obj_s/m_driver$o :	$(srcdir)/m_driver.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_driver.c -o ../obj_s/m_driver$o

../obj_s/m_format$o :	$(srcdir)/m_format.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_format.c -o ../obj_s/m_format$o

../obj_s/m_global$o :	$(srcdir)/m_global.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_global.c -o ../obj_s/m_global$o

../obj_s/m_hook$o :	$(srcdir)/m_hook.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_hook.c -o ../obj_s/m_hook$o

../obj_s/m_item_cur$o :	$(srcdir)/m_item_cur.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_cur.c -o ../obj_s/m_item_cur$o

../obj_s/m_item_nam$o :	$(srcdir)/m_item_nam.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_nam.c -o ../obj_s/m_item_nam$o

../obj_s/m_item_new$o :	$(srcdir)/m_item_new.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_new.c -o ../obj_s/m_item_new$o

../obj_s/m_item_opt$o :	$(srcdir)/m_item_opt.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_opt.c -o ../obj_s/m_item_opt$o

../obj_s/m_item_top$o :	$(srcdir)/m_item_top.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_top.c -o ../obj_s/m_item_top$o

../obj_s/m_item_use$o :	$(srcdir)/m_item_use.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_use.c -o ../obj_s/m_item_use$o

../obj_s/m_item_val$o :	$(srcdir)/m_item_val.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_val.c -o ../obj_s/m_item_val$o

../obj_s/m_item_vis$o :	$(srcdir)/m_item_vis.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_item_vis.c -o ../obj_s/m_item_vis$o

../obj_s/m_items$o :	$(srcdir)/m_items.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_items.c -o ../obj_s/m_items$o

../obj_s/m_new$o :	$(srcdir)/m_new.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_new.c -o ../obj_s/m_new$o

../obj_s/m_opts$o :	$(srcdir)/m_opts.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_opts.c -o ../obj_s/m_opts$o

../obj_s/m_pad$o :	$(srcdir)/m_pad.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_pad.c -o ../obj_s/m_pad$o

../obj_s/m_pattern$o :	$(srcdir)/m_pattern.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_pattern.c -o ../obj_s/m_pattern$o

../obj_s/m_post$o :	$(srcdir)/m_post.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_post.c -o ../obj_s/m_post$o

../obj_s/m_req_name$o :	$(srcdir)/m_req_name.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_req_name.c -o ../obj_s/m_req_name$o

../obj_s/m_scale$o :	$(srcdir)/m_scale.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_scale.c -o ../obj_s/m_scale$o

../obj_s/m_spacing$o :	$(srcdir)/m_spacing.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_spacing.c -o ../obj_s/m_spacing$o

../obj_s/m_sub$o :	$(srcdir)/m_sub.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_sub.c -o ../obj_s/m_sub$o

../obj_s/m_userptr$o :	$(srcdir)/m_userptr.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_userptr.c -o ../obj_s/m_userptr$o

../obj_s/m_win$o :	$(srcdir)/m_win.c \
			$(MENU_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/m_win.c -o ../obj_s/m_win$o


# generated by mk-hdr.awk
#  subset:     ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#  compat:     yes

${DESTDIR}${includedir} :
	sh ${srcdir}/../mkdirs.sh $@

install \
install.libs \
install.includes :: ${AUTO_SRC} ${DESTDIR}${includedir} \
		$(srcdir)/eti.h \
		$(srcdir)/menu.h
	@ (cd ${DESTDIR}${includedir} && rm -f eti.h) ; ../headers.sh ${INSTALL_DATA} ${DESTDIR}${includedir} ${srcdir} $(srcdir)/eti.h
	@ (cd ${DESTDIR}${includedir} && rm -f menu.h) ; ../headers.sh ${INSTALL_DATA} ${DESTDIR}${includedir} ${srcdir} $(srcdir)/menu.h

uninstall \
uninstall.libs \
uninstall.includes ::
	-@ (cd ${DESTDIR}${includedir} && rm -f eti.h)
	-@ (cd ${DESTDIR}${includedir} && rm -f menu.h)
depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
