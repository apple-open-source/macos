# $Id: Makefile.in,v 1.115 2015/08/05 23:15:41 tom Exp $
##############################################################################
# Copyright (c) 1998-2014,2015 Free Software Foundation, Inc.                #
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
# Makefile for ncurses tests.

# turn off _all_ suffix rules; we'll generate our own
.SUFFIXES:

SHELL		= /bin/sh




destdir		=
x		= 
o		= .o

MODEL		= ../obj_s
top_srcdir	= ..
srcdir		= .
prefix		= /usr/local
exec_prefix	= ${prefix}
datarootdir	= /usr/share

bindir		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
includedir	= ${prefix}/include
includesubdir	= /ncursesw

BINDIR		= $(DESTDIR)$(bindir)
LIBDIR		= $(DESTDIR)$(libdir)
INCLUDEDIR	= $(DESTDIR)$(includedir)$(includesubdir)

PACKAGE		= ncursesw

LIBTOOL		= 
LIBTOOL_OPTS	= 
LIBTOOL_CLEAN	= 
LIBTOOL_COMPILE	= 
LIBTOOL_LINK	= ${CC}

INSTALL		= /usr/bin/install -c
INSTALL_PROG	= ${INSTALL}
transform	= s,x,x,
TRANSFORM	= sed 's/$x$$//'|sed '$(transform)'|sed 's/$$/$x/'

CTAGS		= ctags
ETAGS		= 

CC		= gcc
CPP		= gcc -E

CFLAGS		= -O2 -no-cpp-precomp --param max-inline-insns-single=1200 
CPPFLAGS	= -I. -I$(srcdir) -I../test -DHAVE_CONFIG_H -I. -I../include   -D_DARWIN_C_SOURCE -DNDEBUG

CCFLAGS		= $(CPPFLAGS) $(CFLAGS)

CFLAGS_LIBTOOL	= $(CCFLAGS)
CFLAGS_NORMAL	= $(CCFLAGS)
CFLAGS_DEBUG	= $(CCFLAGS) -g -DTRACE
CFLAGS_PROFILE	= $(CCFLAGS) -pg
CFLAGS_SHARED	= $(CCFLAGS) -dynamic

CFLAGS_DEFAULT	= $(CFLAGS_SHARED)

REL_VERSION	= 5.4.9
ABI_VERSION	= 5.4
LOCAL_LIBDIR	= /Users/kevans/aapl-git/ncurses/ncurses/lib
LOCAL_LIBS	= ../lib/libformw.dylib ../lib/libmenuw.dylib ../lib/libpanelw.dylib ../lib/libncursesw.dylib 
MATH_LIB	= -lm
TEST_LIBS	= -lutil

LD		= ld
LINK		=  $(LIBTOOL_LINK) $(CFLAGS)

LDFLAGS		=    -Wl,-search_paths_first

LDFLAGS_LIBTOOL	= $(LDFLAGS) $(CFLAGS_LIBTOOL)
LDFLAGS_NORMAL	= $(LDFLAGS) $(CFLAGS_NORMAL)
LDFLAGS_DEBUG	= $(LDFLAGS) $(CFLAGS_DEBUG)
LDFLAGS_PROFILE	= $(LDFLAGS) $(CFLAGS_PROFILE)
LDFLAGS_SHARED	= $(LDFLAGS) $(CFLAGS_SHARED) 

TEST_ARGS	=  -L../lib -lformw -lmenuw -lpanelw -lncursesw  

# use these for linking with all of the libraries
LIBS_DEFAULT	= $(TEST_ARGS)  $(TEST_LIBS) $(MATH_LIB)
LDFLAGS_DEFAULT	= $(LDFLAGS_SHARED) $(LIBS_DEFAULT)

# use these for linking with the (n)curses library and possibly pthreads
LIBS_THREADS	= `echo "$(TEST_ARGS) " | sed -e 's/-lform.*-lpanel[^ ]*//'` $(TEST_LIBS) $(MATH_LIB) 
LDFLAGS_THREADS	= $(LDFLAGS_SHARED) $(LIBS_THREADS)

# use these for linking with the (n)curses library
LIBS_CURSES	= `echo "$(TEST_ARGS) " | sed -e 's/-lform.*-lpanel[^ ]*//'` $(TEST_LIBS) $(MATH_LIB)
LDFLAGS_CURSES	= $(LDFLAGS_SHARED) $(LIBS_CURSES)

# use these for linking with the tinfo library if we have it, or curses library if not
LIBS_TINFO	=  -lncursesw   $(TEST_LIBS) $(MATH_LIB)
LDFLAGS_TINFO	= -L../lib $(LDFLAGS_SHARED) $(LIBS_TINFO)

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lform -lmenu -lpanel -lncurses 

HEADER_DEPS	= \
	../include/curses.h \
	../include/term.h

# The rest is generated from the "programs" and "modules" files...
# generated by mk-test.awk

TESTS	= $(destdir)background$x \
	  $(destdir)blue$x \
	  $(destdir)bs$x \
	  $(destdir)cardfile$x \
	  $(destdir)chgat$x \
	  $(destdir)clip_printw$x \
	  $(destdir)color_set$x \
	  $(destdir)demo_altkeys$x \
	  $(destdir)demo_defkey$x \
	  $(destdir)demo_forms$x \
	  $(destdir)demo_keyok$x \
	  $(destdir)demo_menus$x \
	  $(destdir)demo_panels$x \
	  $(destdir)demo_termcap$x \
	  $(destdir)demo_terminfo$x \
	  $(destdir)ditto$x \
	  $(destdir)dots$x \
	  $(destdir)dots_curses$x \
	  $(destdir)dots_mvcur$x \
	  $(destdir)dots_termcap$x \
	  $(destdir)echochar$x \
	  $(destdir)filter$x \
	  $(destdir)firework$x \
	  $(destdir)firstlast$x \
	  $(destdir)foldkeys$x \
	  $(destdir)form_driver_w$x \
	  $(destdir)gdc$x \
	  $(destdir)hanoi$x \
	  $(destdir)hashtest$x \
	  $(destdir)inch_wide$x \
	  $(destdir)inchs$x \
	  $(destdir)ins_wide$x \
	  $(destdir)insdelln$x \
	  $(destdir)inserts$x \
	  $(destdir)key_names$x \
	  $(destdir)keynames$x \
	  $(destdir)knight$x \
	  $(destdir)lrtest$x \
	  $(destdir)movewindow$x \
	  $(destdir)ncurses$x \
	  $(destdir)newdemo$x \
	  $(destdir)railroad$x \
	  $(destdir)rain$x \
	  $(destdir)redraw$x \
	  $(destdir)savescreen$x \
	  $(destdir)tclock$x \
	  $(destdir)test_add_wchstr$x \
	  $(destdir)test_addchstr$x \
	  $(destdir)test_addstr$x \
	  $(destdir)test_addwstr$x \
	  $(destdir)test_arrays$x \
	  $(destdir)test_get_wstr$x \
	  $(destdir)test_getstr$x \
	  $(destdir)test_instr$x \
	  $(destdir)test_inwstr$x \
	  $(destdir)test_opaque$x \
	  $(destdir)test_setupterm$x \
	  $(destdir)test_vid_puts$x \
	  $(destdir)test_vidputs$x \
	  $(destdir)testaddch$x \
	  $(destdir)testcurs$x \
	  $(destdir)testscanw$x \
	  $(destdir)view$x \
	  $(destdir)worm$x \
	  $(destdir)xmas$x

all: $(TESTS)

sources:

tags:
	$(CTAGS) *.[ch]

# no libraries here
libs \
install.libs \
uninstall.libs:

install \
install.test \
uninstall \
uninstall.test:

mostlyclean ::
	-rm -f core tags TAGS *~ *.bak *.i *.ln *.atac trace

clean :: mostlyclean
	-$(SHELL) -c "if test -n '$x' ; then $(MAKE) clean x=''; fi"
	-rm -rf *$o screendump *.lis $(TESTS) .libs *.dSYM

distclean :: clean
	-rm -f Makefile ncurses_cfg.h config.status config.log

realclean :: distclean

lint:
	$(SHELL) -c 'for N in $(TESTS); do echo LINT:$$N; $(LINT) $(LINT_OPTS) $(CPPFLAGS) $(srcdir)/$$N.c $(LINT_LIBS); done'
$(BINDIR) :
	mkdir -p $@

$(destdir)background$x: $(MODEL)/background$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/background$o $(LDFLAGS_CURSES)

$(destdir)blue$x: $(MODEL)/blue$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/blue$o $(LDFLAGS_CURSES)

$(destdir)bs$x: $(MODEL)/bs$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/bs$o $(LDFLAGS_CURSES)

$(destdir)cardfile$x: $(MODEL)/cardfile$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/cardfile$o $(LDFLAGS_DEFAULT)

$(destdir)chgat$x: $(MODEL)/chgat$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/chgat$o $(LDFLAGS_CURSES)

$(destdir)clip_printw$x: $(MODEL)/clip_printw$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/clip_printw$o $(LDFLAGS_CURSES)

$(destdir)color_set$x: $(MODEL)/color_set$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/color_set$o $(LDFLAGS_CURSES)

$(destdir)demo_altkeys$x: $(MODEL)/demo_altkeys$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_altkeys$o $(LDFLAGS_CURSES)

$(destdir)demo_defkey$x: $(MODEL)/demo_defkey$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_defkey$o $(LDFLAGS_CURSES)

$(destdir)demo_forms$x: $(MODEL)/demo_forms$o $(MODEL)/edit_field$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_forms$o $(MODEL)/edit_field$o $(LDFLAGS_DEFAULT)

$(destdir)demo_keyok$x: $(MODEL)/demo_keyok$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_keyok$o $(LDFLAGS_CURSES)

$(destdir)demo_menus$x: $(MODEL)/demo_menus$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_menus$o $(LDFLAGS_DEFAULT)

$(destdir)demo_panels$x: $(MODEL)/demo_panels$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_panels$o $(LDFLAGS_DEFAULT)

$(destdir)demo_termcap$x: $(MODEL)/demo_termcap$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_termcap$o $(LDFLAGS_TINFO)

$(destdir)demo_terminfo$x: $(MODEL)/demo_terminfo$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/demo_terminfo$o $(LDFLAGS_TINFO)

$(destdir)ditto$x: $(MODEL)/ditto$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/ditto$o $(LDFLAGS_THREADS)

$(destdir)dots$x: $(MODEL)/dots$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/dots$o $(LDFLAGS_TINFO)

$(destdir)dots_curses$x: $(MODEL)/dots_curses$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/dots_curses$o $(LDFLAGS_CURSES)

$(destdir)dots_mvcur$x: $(MODEL)/dots_mvcur$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/dots_mvcur$o $(LDFLAGS_CURSES)

$(destdir)dots_termcap$x: $(MODEL)/dots_termcap$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/dots_termcap$o $(LDFLAGS_TINFO)

$(destdir)echochar$x: $(MODEL)/echochar$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/echochar$o $(LDFLAGS_CURSES)

$(destdir)filter$x: $(MODEL)/filter$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/filter$o $(LDFLAGS_CURSES)

$(destdir)firework$x: $(MODEL)/firework$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/firework$o $(LDFLAGS_CURSES)

$(destdir)firstlast$x: $(MODEL)/firstlast$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/firstlast$o $(LDFLAGS_DEFAULT)

$(destdir)foldkeys$x: $(MODEL)/foldkeys$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/foldkeys$o $(LDFLAGS_CURSES)

$(destdir)form_driver_w$x: $(MODEL)/form_driver_w$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/form_driver_w$o $(LDFLAGS_DEFAULT)

$(destdir)gdc$x: $(MODEL)/gdc$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/gdc$o $(LDFLAGS_CURSES)

$(destdir)hanoi$x: $(MODEL)/hanoi$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/hanoi$o $(LDFLAGS_CURSES)

$(destdir)hashtest$x: $(MODEL)/hashtest$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/hashtest$o $(LDFLAGS_CURSES)

$(destdir)inch_wide$x: $(MODEL)/inch_wide$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/inch_wide$o $(LDFLAGS_CURSES)

$(destdir)inchs$x: $(MODEL)/inchs$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/inchs$o $(LDFLAGS_CURSES)

$(destdir)ins_wide$x: $(MODEL)/ins_wide$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/ins_wide$o $(LDFLAGS_CURSES)

$(destdir)insdelln$x: $(MODEL)/insdelln$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/insdelln$o $(LDFLAGS_CURSES)

$(destdir)inserts$x: $(MODEL)/inserts$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/inserts$o $(LDFLAGS_CURSES)

$(destdir)key_names$x: $(MODEL)/key_names$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/key_names$o $(LDFLAGS_CURSES)

$(destdir)keynames$x: $(MODEL)/keynames$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/keynames$o $(LDFLAGS_CURSES)

$(destdir)knight$x: $(MODEL)/knight$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/knight$o $(LDFLAGS_CURSES)

$(destdir)lrtest$x: $(MODEL)/lrtest$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/lrtest$o $(LDFLAGS_CURSES)

$(destdir)movewindow$x: $(MODEL)/movewindow$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/movewindow$o $(LDFLAGS_CURSES)

$(destdir)ncurses$x: $(MODEL)/ncurses$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/ncurses$o $(LDFLAGS_DEFAULT)

$(destdir)newdemo$x: $(MODEL)/newdemo$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/newdemo$o $(LDFLAGS_CURSES)

$(destdir)railroad$x: $(MODEL)/railroad$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/railroad$o $(LDFLAGS_TINFO)

$(destdir)rain$x: $(MODEL)/rain$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/rain$o $(LDFLAGS_THREADS)

$(destdir)redraw$x: $(MODEL)/redraw$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/redraw$o $(LDFLAGS_CURSES)

$(destdir)savescreen$x: $(MODEL)/savescreen$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/savescreen$o $(LDFLAGS_CURSES)

$(destdir)tclock$x: $(MODEL)/tclock$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/tclock$o $(LDFLAGS_CURSES)

$(destdir)test_add_wchstr$x: $(MODEL)/test_add_wchstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_add_wchstr$o $(LDFLAGS_CURSES)

$(destdir)test_addchstr$x: $(MODEL)/test_addchstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_addchstr$o $(LDFLAGS_CURSES)

$(destdir)test_addstr$x: $(MODEL)/test_addstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_addstr$o $(LDFLAGS_CURSES)

$(destdir)test_addwstr$x: $(MODEL)/test_addwstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_addwstr$o $(LDFLAGS_CURSES)

$(destdir)test_arrays$x: $(MODEL)/test_arrays$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_arrays$o $(LDFLAGS_TINFO)

$(destdir)test_get_wstr$x: $(MODEL)/test_get_wstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_get_wstr$o $(LDFLAGS_CURSES)

$(destdir)test_getstr$x: $(MODEL)/test_getstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_getstr$o $(LDFLAGS_CURSES)

$(destdir)test_instr$x: $(MODEL)/test_instr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_instr$o $(LDFLAGS_CURSES)

$(destdir)test_inwstr$x: $(MODEL)/test_inwstr$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_inwstr$o $(LDFLAGS_CURSES)

$(destdir)test_opaque$x: $(MODEL)/test_opaque$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_opaque$o $(LDFLAGS_CURSES)

$(destdir)test_setupterm$x: $(MODEL)/test_setupterm$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_setupterm$o $(LDFLAGS_CURSES)

$(destdir)test_vid_puts$x: $(MODEL)/test_vid_puts$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_vid_puts$o $(LDFLAGS_CURSES)

$(destdir)test_vidputs$x: $(MODEL)/test_vidputs$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/test_vidputs$o $(LDFLAGS_CURSES)

$(destdir)testaddch$x: $(MODEL)/testaddch$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/testaddch$o $(LDFLAGS_CURSES)

$(destdir)testcurs$x: $(MODEL)/testcurs$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/testcurs$o $(LDFLAGS_CURSES)

$(destdir)testscanw$x: $(MODEL)/testscanw$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/testscanw$o $(LDFLAGS_CURSES)

$(destdir)view$x: $(MODEL)/view$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/view$o $(LDFLAGS_CURSES)

$(destdir)worm$x: $(MODEL)/worm$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/worm$o $(LDFLAGS_THREADS)

$(destdir)xmas$x: $(MODEL)/xmas$o $(LOCAL_LIBS)
	$(LINK) -o $@ $(MODEL)/xmas$o $(LDFLAGS_CURSES)

# Generated by CF_LIB_RULES
resulting.map: 
	sed  -e "s/NCURSES\([WT]\+\)\?_/NCURSES\15.4_/g" -e "/deprecated in ABI5.4/d" <  >$@

clean::
	rm -f resulting.map

# generated by mk-0th.awk
#   libname:    testw
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(srcdir)/background.c \
	$(srcdir)/blue.c \
	$(srcdir)/bs.c \
	$(srcdir)/cardfile.c \
	$(srcdir)/chgat.c \
	$(srcdir)/clip_printw.c \
	$(srcdir)/color_set.c \
	$(srcdir)/demo_altkeys.c \
	$(srcdir)/demo_defkey.c \
	$(srcdir)/demo_forms.c \
	$(srcdir)/demo_keyok.c \
	$(srcdir)/demo_menus.c \
	$(srcdir)/demo_panels.c \
	$(srcdir)/demo_termcap.c \
	$(srcdir)/demo_terminfo.c \
	$(srcdir)/ditto.c \
	$(srcdir)/dots.c \
	$(srcdir)/dots_curses.c \
	$(srcdir)/dots_mvcur.c \
	$(srcdir)/dots_termcap.c \
	$(srcdir)/echochar.c \
	$(srcdir)/edit_field.c \
	$(srcdir)/filter.c \
	$(srcdir)/firework.c \
	$(srcdir)/firstlast.c \
	$(srcdir)/foldkeys.c \
	$(srcdir)/form_driver_w.c \
	$(srcdir)/gdc.c \
	$(srcdir)/hanoi.c \
	$(srcdir)/hashtest.c \
	$(srcdir)/inch_wide.c \
	$(srcdir)/inchs.c \
	$(srcdir)/ins_wide.c \
	$(srcdir)/insdelln.c \
	$(srcdir)/inserts.c \
	$(srcdir)/key_names.c \
	$(srcdir)/keynames.c \
	$(srcdir)/knight.c \
	$(srcdir)/lrtest.c \
	$(srcdir)/movewindow.c \
	$(srcdir)/ncurses.c \
	$(srcdir)/newdemo.c \
	$(srcdir)/railroad.c \
	$(srcdir)/rain.c \
	$(srcdir)/redraw.c \
	$(srcdir)/savescreen.c \
	$(srcdir)/tclock.c \
	$(srcdir)/test_add_wchstr.c \
	$(srcdir)/test_addchstr.c \
	$(srcdir)/test_addstr.c \
	$(srcdir)/test_addwstr.c \
	$(srcdir)/test_arrays.c \
	$(srcdir)/test_get_wstr.c \
	$(srcdir)/test_getstr.c \
	$(srcdir)/test_instr.c \
	$(srcdir)/test_inwstr.c \
	$(srcdir)/test_opaque.c \
	$(srcdir)/test_setupterm.c \
	$(srcdir)/test_vid_puts.c \
	$(srcdir)/test_vidputs.c \
	$(srcdir)/testaddch.c \
	$(srcdir)/testcurs.c \
	$(srcdir)/testscanw.c \
	$(srcdir)/view.c \
	$(srcdir)/worm.c \
	$(srcdir)/xmas.c

lintlib :
	@echo no action needed

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          testw
#  traces:        DEBUG
#  MODEL:         SHARED
#  CXX_MODEL:     SHARED
#  model:         obj_s
#  prefix:        lib
#  suffix:        .dylib
#  subset:        ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#  driver:        no
#  ShlibVer:      abi
#  ShlibVerInfix: yes
#  SymLink:       ln -s -f
#  TermlibRoot:   ncursesw
#  TermlibSuffix: .dylib
#  ReLink:        yes
#  DoLinks:       yes
#  rmSoLocs:      no
#  ldconfig:      
#  overwrite:     no
#  depend:        ../include/ncurses_cfg.h ./test.priv.h
#  host:          arm-apple-darwin23.0.0

SHARED_OBJS = \
	../obj_s/background$o \
	../obj_s/blue$o \
	../obj_s/bs$o \
	../obj_s/cardfile$o \
	../obj_s/chgat$o \
	../obj_s/clip_printw$o \
	../obj_s/color_set$o \
	../obj_s/demo_altkeys$o \
	../obj_s/demo_defkey$o \
	../obj_s/demo_forms$o \
	../obj_s/demo_keyok$o \
	../obj_s/demo_menus$o \
	../obj_s/demo_panels$o \
	../obj_s/demo_termcap$o \
	../obj_s/demo_terminfo$o \
	../obj_s/ditto$o \
	../obj_s/dots$o \
	../obj_s/dots_curses$o \
	../obj_s/dots_mvcur$o \
	../obj_s/dots_termcap$o \
	../obj_s/echochar$o \
	../obj_s/edit_field$o \
	../obj_s/filter$o \
	../obj_s/firework$o \
	../obj_s/firstlast$o \
	../obj_s/foldkeys$o \
	../obj_s/form_driver_w$o \
	../obj_s/gdc$o \
	../obj_s/hanoi$o \
	../obj_s/hashtest$o \
	../obj_s/inch_wide$o \
	../obj_s/inchs$o \
	../obj_s/ins_wide$o \
	../obj_s/insdelln$o \
	../obj_s/inserts$o \
	../obj_s/key_names$o \
	../obj_s/keynames$o \
	../obj_s/knight$o \
	../obj_s/lrtest$o \
	../obj_s/movewindow$o \
	../obj_s/ncurses$o \
	../obj_s/newdemo$o \
	../obj_s/railroad$o \
	../obj_s/rain$o \
	../obj_s/redraw$o \
	../obj_s/savescreen$o \
	../obj_s/tclock$o \
	../obj_s/test_add_wchstr$o \
	../obj_s/test_addchstr$o \
	../obj_s/test_addstr$o \
	../obj_s/test_addwstr$o \
	../obj_s/test_arrays$o \
	../obj_s/test_get_wstr$o \
	../obj_s/test_getstr$o \
	../obj_s/test_instr$o \
	../obj_s/test_inwstr$o \
	../obj_s/test_opaque$o \
	../obj_s/test_setupterm$o \
	../obj_s/test_vid_puts$o \
	../obj_s/test_vidputs$o \
	../obj_s/testaddch$o \
	../obj_s/testcurs$o \
	../obj_s/testscanw$o \
	../obj_s/view$o \
	../obj_s/worm$o \
	../obj_s/xmas$o

$(SHARED_OBJS) : ../include/ncurses_cfg.h ./test.priv.h

mostlyclean::
	-rm -f $(SHARED_OBJS)

clean ::
	-rm -f $(SHARED_OBJS)

# generated by mk-2nd.awk
#   model:      obj_s
#   MODEL:      SHARED
#   echo:       yes
#   subset:     ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#   crenames:   yes
#   cxxrenames: 
#   traces:     DEBUG
#   srcdir:     .

../obj_s/background$o :	$(srcdir)/background.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/background.c -o ../obj_s/background$o

../obj_s/blue$o :	$(srcdir)/blue.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/blue.c -o ../obj_s/blue$o

../obj_s/bs$o :	$(srcdir)/bs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/bs.c -o ../obj_s/bs$o

../obj_s/cardfile$o :	$(srcdir)/cardfile.c \
			$(HEADER_DEPS) \
			../include/panel.h \
			../include/form.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/cardfile.c -o ../obj_s/cardfile$o

../obj_s/chgat$o :	$(srcdir)/chgat.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/chgat.c -o ../obj_s/chgat$o

../obj_s/clip_printw$o :	$(srcdir)/clip_printw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/clip_printw.c -o ../obj_s/clip_printw$o

../obj_s/color_set$o :	$(srcdir)/color_set.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/color_set.c -o ../obj_s/color_set$o

../obj_s/demo_altkeys$o :	$(srcdir)/demo_altkeys.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_altkeys.c -o ../obj_s/demo_altkeys$o

../obj_s/demo_defkey$o :	$(srcdir)/demo_defkey.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_defkey.c -o ../obj_s/demo_defkey$o

../obj_s/demo_forms$o :	$(srcdir)/demo_forms.c \
			$(HEADER_DEPS) \
			$(srcdir)/edit_field.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_forms.c -o ../obj_s/demo_forms$o

../obj_s/demo_keyok$o :	$(srcdir)/demo_keyok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_keyok.c -o ../obj_s/demo_keyok$o

../obj_s/demo_menus$o :	$(srcdir)/demo_menus.c \
			$(HEADER_DEPS) \
			../include/menu.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_menus.c -o ../obj_s/demo_menus$o

../obj_s/demo_panels$o :	$(srcdir)/demo_panels.c \
			$(HEADER_DEPS) \
			../include/panel.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_panels.c -o ../obj_s/demo_panels$o

../obj_s/demo_termcap$o :	$(srcdir)/demo_termcap.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_termcap.c -o ../obj_s/demo_termcap$o

../obj_s/demo_terminfo$o :	$(srcdir)/demo_terminfo.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/demo_terminfo.c -o ../obj_s/demo_terminfo$o

../obj_s/ditto$o :	$(srcdir)/ditto.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/ditto.c -o ../obj_s/ditto$o

../obj_s/dots$o :	$(srcdir)/dots.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/dots.c -o ../obj_s/dots$o

../obj_s/dots_curses$o :	$(srcdir)/dots_curses.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/dots_curses.c -o ../obj_s/dots_curses$o

../obj_s/dots_mvcur$o :	$(srcdir)/dots_mvcur.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/dots_mvcur.c -o ../obj_s/dots_mvcur$o

../obj_s/dots_termcap$o :	$(srcdir)/dots_termcap.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/dots_termcap.c -o ../obj_s/dots_termcap$o

../obj_s/echochar$o :	$(srcdir)/echochar.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/echochar.c -o ../obj_s/echochar$o

../obj_s/edit_field$o :	$(srcdir)/edit_field.c \
			$(HEADER_DEPS) \
			$(srcdir)/edit_field.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/edit_field.c -o ../obj_s/edit_field$o

../obj_s/filter$o :	$(srcdir)/filter.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/filter.c -o ../obj_s/filter$o

../obj_s/firework$o :	$(srcdir)/firework.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/firework.c -o ../obj_s/firework$o

../obj_s/firstlast$o :	$(srcdir)/firstlast.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/firstlast.c -o ../obj_s/firstlast$o

../obj_s/foldkeys$o :	$(srcdir)/foldkeys.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/foldkeys.c -o ../obj_s/foldkeys$o

../obj_s/form_driver_w$o :	$(srcdir)/form_driver_w.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/form_driver_w.c -o ../obj_s/form_driver_w$o

../obj_s/gdc$o :	$(srcdir)/gdc.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/gdc.c -o ../obj_s/gdc$o

../obj_s/hanoi$o :	$(srcdir)/hanoi.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/hanoi.c -o ../obj_s/hanoi$o

../obj_s/hashtest$o :	$(srcdir)/hashtest.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/hashtest.c -o ../obj_s/hashtest$o

../obj_s/inch_wide$o :	$(srcdir)/inch_wide.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/inch_wide.c -o ../obj_s/inch_wide$o

../obj_s/inchs$o :	$(srcdir)/inchs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/inchs.c -o ../obj_s/inchs$o

../obj_s/ins_wide$o :	$(srcdir)/ins_wide.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/ins_wide.c -o ../obj_s/ins_wide$o

../obj_s/insdelln$o :	$(srcdir)/insdelln.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/insdelln.c -o ../obj_s/insdelln$o

../obj_s/inserts$o :	$(srcdir)/inserts.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/inserts.c -o ../obj_s/inserts$o

../obj_s/key_names$o :	$(srcdir)/key_names.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/key_names.c -o ../obj_s/key_names$o

../obj_s/keynames$o :	$(srcdir)/keynames.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/keynames.c -o ../obj_s/keynames$o

../obj_s/knight$o :	$(srcdir)/knight.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/knight.c -o ../obj_s/knight$o

../obj_s/lrtest$o :	$(srcdir)/lrtest.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/lrtest.c -o ../obj_s/lrtest$o

../obj_s/movewindow$o :	$(srcdir)/movewindow.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/movewindow.c -o ../obj_s/movewindow$o

../obj_s/ncurses$o :	$(srcdir)/ncurses.c \
			$(HEADER_DEPS) \
			../include/panel.h \
			../include/menu.h \
			../include/form.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/ncurses.c -o ../obj_s/ncurses$o

../obj_s/newdemo$o :	$(srcdir)/newdemo.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/newdemo.c -o ../obj_s/newdemo$o

../obj_s/railroad$o :	$(srcdir)/railroad.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/railroad.c -o ../obj_s/railroad$o

../obj_s/rain$o :	$(srcdir)/rain.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/rain.c -o ../obj_s/rain$o

../obj_s/redraw$o :	$(srcdir)/redraw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/redraw.c -o ../obj_s/redraw$o

../obj_s/savescreen$o :	$(srcdir)/savescreen.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/savescreen.c -o ../obj_s/savescreen$o

../obj_s/tclock$o :	$(srcdir)/tclock.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/tclock.c -o ../obj_s/tclock$o

../obj_s/test_add_wchstr$o :	$(srcdir)/test_add_wchstr.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_add_wchstr.c -o ../obj_s/test_add_wchstr$o

../obj_s/test_addchstr$o :	$(srcdir)/test_addchstr.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_addchstr.c -o ../obj_s/test_addchstr$o

../obj_s/test_addstr$o :	$(srcdir)/test_addstr.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_addstr.c -o ../obj_s/test_addstr$o

../obj_s/test_addwstr$o :	$(srcdir)/test_addwstr.c \
			$(HEADER_DEPS) \
			$(srcdir)/linedata.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_addwstr.c -o ../obj_s/test_addwstr$o

../obj_s/test_arrays$o :	$(srcdir)/test_arrays.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_arrays.c -o ../obj_s/test_arrays$o

../obj_s/test_get_wstr$o :	$(srcdir)/test_get_wstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_get_wstr.c -o ../obj_s/test_get_wstr$o

../obj_s/test_getstr$o :	$(srcdir)/test_getstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_getstr.c -o ../obj_s/test_getstr$o

../obj_s/test_instr$o :	$(srcdir)/test_instr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_instr.c -o ../obj_s/test_instr$o

../obj_s/test_inwstr$o :	$(srcdir)/test_inwstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_inwstr.c -o ../obj_s/test_inwstr$o

../obj_s/test_opaque$o :	$(srcdir)/test_opaque.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_opaque.c -o ../obj_s/test_opaque$o

../obj_s/test_setupterm$o :	$(srcdir)/test_setupterm.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_setupterm.c -o ../obj_s/test_setupterm$o

../obj_s/test_vid_puts$o :	$(srcdir)/test_vid_puts.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_vid_puts.c -o ../obj_s/test_vid_puts$o

../obj_s/test_vidputs$o :	$(srcdir)/test_vidputs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/test_vidputs.c -o ../obj_s/test_vidputs$o

../obj_s/testaddch$o :	$(srcdir)/testaddch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/testaddch.c -o ../obj_s/testaddch$o

../obj_s/testcurs$o :	$(srcdir)/testcurs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/testcurs.c -o ../obj_s/testcurs$o

../obj_s/testscanw$o :	$(srcdir)/testscanw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/testscanw.c -o ../obj_s/testscanw$o

../obj_s/view$o :	$(srcdir)/view.c \
			$(HEADER_DEPS) \
			$(srcdir)/widechars.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/view.c -o ../obj_s/view$o

../obj_s/worm$o :	$(srcdir)/worm.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/worm.c -o ../obj_s/worm$o

../obj_s/xmas$o :	$(srcdir)/xmas.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../test/xmas.c -o ../obj_s/xmas$o

depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
