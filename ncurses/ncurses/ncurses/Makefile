# $Id: Makefile.in,v 1.147 2015/08/05 23:15:41 tom Exp $
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
# Makefile for ncurses source code.
#
# This makes the following:
#	programs
#	includes
#	libraries (normal/debug/profile/shared)
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

MODEL		= shared
DESTDIR		= 
top_srcdir	= ..
srcdir		= .
prefix		= /usr/local
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
includedir	= ${prefix}/include
datarootdir	= /usr/share
datadir		= ${datarootdir}
includesubdir	= /ncursesw

INCLUDEDIR	= $(DESTDIR)$(includedir)$(includesubdir)

PACKAGE		= ncursesw

LIBTOOL		= 
LIBTOOL_OPTS	=  
LIBTOOL_CLEAN	= 
LIBTOOL_COMPILE	= 
LIBTOOL_LINK	= ${CC}
LIBTOOL_INSTALL	= 
LIBTOOL_UNINSTALL = 
LT_UNDEF	= 

INSTALL		= /usr/bin/install -c
INSTALL_LIB	= /usr/bin/install -c -m 644
INSTALL_PROG	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

AR		= ar
ARFLAGS		= -curv
AWK		= awk
LD		= ld
LN_S		= ln -s -f
NM		= nm

CTAGS		= ctags
ETAGS		= 

CC		= gcc
CPP		= gcc -E
CFLAGS		= -O2 -no-cpp-precomp --param max-inline-insns-single=1200

INCDIR		= $(srcdir)/../include
CPPFLAGS	= -DHAVE_CONFIG_H -I. -I../include   -D_DARWIN_C_SOURCE -DNDEBUG

CCFLAGS		= $(CPPFLAGS) $(CFLAGS)

BUILD_CPPFLAGS	= -DHAVE_CONFIG_H -DUSE_BUILD_CC -I../ncurses -I$(srcdir) -I../include -I$(INCDIR) ${CPPFLAGS} 
BUILD_CC	= ${CC}
BUILD_CCFLAGS	= ${CFLAGS}
BUILD_LDFLAGS	= ${LDFLAGS}
BUILD_LIBS	= ${LIBS}

# The executables built in this directory are used for generating source that
# is compiled into the build, or are test-programs that are not installed.

BUILD_EXEEXT	= $x
x		= 

CFLAGS_LIBTOOL	= $(CCFLAGS)
CFLAGS_NORMAL	= $(CCFLAGS)
CFLAGS_DEBUG	= $(CCFLAGS) -g -DTRACE
CFLAGS_PROFILE	= $(CCFLAGS) -pg
CFLAGS_SHARED	= $(CCFLAGS) -dynamic

CFLAGS_DEFAULT	= $(CFLAGS_SHARED)

LINK		= $(LIBTOOL)
LDFLAGS		=  -Wl,-search_paths_first  

SHLIB_DIRS	= -L../lib
SHLIB_LIST	= $(SHLIB_DIRS) 
TINFO_LIST	= $(SHLIB_DIRS) 
TICS_LIST	= $(SHLIB_DIRS) 

RPATH_LIST	= ${libdir}
RESULTING_SYMS	= 
VERSIONED_SYMS	= 
MK_SHARED_LIB	= ${CC} ${CFLAGS} -dynamiclib -install_name ${libdir}/`basename $@` -compatibility_version ${ABI_VERSION} -current_version ${ABI_VERSION} -o $@

NCURSES_MAJOR	= 6
NCURSES_MINOR	= 0
REL_VERSION	= 5.4.9
ABI_VERSION	= 5.4

RANLIB		= ranlib

LIBRARIES	=  ../lib/libncursesw.${ABI_VERSION}.dylib

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lncurses 

FALLBACK_LIST	= 

USE_BIG_STRINGS	= 1
TERMINFO_CAPS	= $(top_srcdir)/include/Caps

TERMINFO	= /usr/share/terminfo
TERMINFO_SRC	= ${top_srcdir}/misc/terminfo.src
TIC_PATH	= /usr/bin/tic

AUTO_SRC = \
	./codes.c \
	./comp_captab.c \
	./expanded.c \
	./fallback.c \
	./lib_gen.c \
	./lib_keyname.c \
	./link_test.c \
	./names.c \
	./unctrl.c \
	init_keytry.h \
	keys.list

HEADER_DEPS	= \
	$(srcdir)/curses.priv.h \
	../include/ncurses_dll.h \
	../include/ncurses_cfg.h \
	../include/curses.h \
	$(INCDIR)/nc_panel.h \
	../include/term.h \
	$(INCDIR)/term_entry.h \
	$(INCDIR)/nc_tparm.h \
	$(INCDIR)/nc_alloc.h \
	$(INCDIR)/nc_mingw.h

TEST_DEPS	= ../lib/libncursesw.dylib
TEST_ARGS	=  -L../lib -lformw -lmenuw -lpanelw -lncursesw   
TEST_LDFLAGS	=  $(TEST_ARGS)    -Wl,-search_paths_first

TEST_PROGS = \
	captoinfo$x \
	hardscroll$x \
	link_test$x \
	hashmap$x \
	lib_mvcur$x

base	 = $(srcdir)/base
serial	 = $(srcdir)/tty
tinfo	 = $(srcdir)/tinfo
trace	 = $(srcdir)/trace
wide	 = $(srcdir)/widechar
win32con = $(srcdir)/win32con

################################################################################
all \
libs ::		$(AUTO_SRC) ../lib $(LIBRARIES)

sources:	$(AUTO_SRC)

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) :
	mkdir -p $@

../lib : ; mkdir $@

./fallback.c : $(tinfo)/MKfallback.sh
	$(SHELL) -e $(tinfo)/MKfallback.sh $(TERMINFO) $(TERMINFO_SRC) $(TIC_PATH) $(FALLBACK_LIST) >$@

./lib_gen.c : $(base)/MKlib_gen.sh ../include/curses.h
	$(SHELL) -e $(base)/MKlib_gen.sh "$(CPP) $(CPPFLAGS)" "$(AWK)" generated <../include/curses.h >$@

init_keytry.h: make_keys$(BUILD_EXEEXT) keys.list
	./make_keys$(BUILD_EXEEXT) keys.list > $@

keys.list :	$(tinfo)/MKkeys_list.sh
	AWK=$(AWK) $(SHELL) $(tinfo)/MKkeys_list.sh $(TERMINFO_CAPS) | sort >$@

make_keys$(BUILD_EXEEXT) : \
		$(tinfo)/make_keys.c \
		names.c
	$(BUILD_CC) -o $@ $(BUILD_CPPFLAGS) $(BUILD_CCFLAGS) $(tinfo)/make_keys.c $(BUILD_LDFLAGS) $(BUILD_LIBS)

make_hash$(BUILD_EXEEXT) : \
		$(tinfo)/make_hash.c \
		../include/hashsize.h
	$(BUILD_CC) -o $@ $(BUILD_CPPFLAGS) $(BUILD_CCFLAGS) $(tinfo)/make_hash.c $(BUILD_LDFLAGS) $(BUILD_LIBS)

./expanded.c : $(srcdir)/curses.priv.h $(serial)/MKexpanded.sh
	$(SHELL) -e $(serial)/MKexpanded.sh "$(CPP)" $(CPPFLAGS) > $@

./comp_captab.c: \
		make_hash$(BUILD_EXEEXT) \
		../include/hashsize.h \
		$(tinfo)/MKcaptab.sh \
		$(tinfo)/MKcaptab.awk
	$(SHELL) -e $(tinfo)/MKcaptab.sh $(AWK) $(USE_BIG_STRINGS) $(tinfo)/MKcaptab.awk $(srcdir)/../include/Caps > $@

./lib_keyname.c: keys.list $(base)/MKkeyname.awk
	$(AWK) -f $(base)/MKkeyname.awk bigstrings=$(USE_BIG_STRINGS) keys.list > $@

./codes.c: $(tinfo)/MKcodes.awk
	$(AWK) -f $(tinfo)/MKcodes.awk bigstrings=$(USE_BIG_STRINGS) $(srcdir)/../include/Caps >$@

./names.c: $(tinfo)/MKnames.awk
	$(AWK) -f $(tinfo)/MKnames.awk bigstrings=$(USE_BIG_STRINGS) $(srcdir)/../include/Caps >$@

./unctrl.c: $(base)/MKunctrl.awk
	echo | $(AWK) -f $(base)/MKunctrl.awk bigstrings=$(USE_BIG_STRINGS) >$@

tags:
	$(CTAGS) *.[ch] */*.[ch]

#TAGS:
#	$(ETAGS) *.[ch] */*.[ch]

mostlyclean ::
	-rm -f core tags TAGS *~ *.bak *.i *.ln *.atac trace
	-rm -f $(TEST_PROGS)

clean :: mostlyclean
	-rm -f $(AUTO_SRC)
	-rm -f make_keys$(BUILD_EXEEXT)
	-rm -f make_hash$(BUILD_EXEEXT)
	-rm -rf .libs *.dSYM *.map

distclean :: clean
	-rm -f Makefile

realclean :: distclean

# These rules are used to allow "make -n" to work on a clean directory-tree
../include/hashsize.h \
../include/parametrized.h \
../include/term.h :
	cd ../include; $(MAKE) $(CF_MFLAGS)

# These rules build test-programs for the modules that have test-drivers
test_progs : $(TEST_PROGS)

./link_test.c : $(base)/MKlib_gen.sh ../include/curses.h
	$(SHELL) -e $(base)/MKlib_gen.sh "$(CPP) $(CPPFLAGS)" "$(AWK)" implemented <../include/curses.h >$@

captoinfo$x : $(tinfo)/captoinfo.c $(TEST_DEPS)
	 $(LIBTOOL_LINK) -o $@ $(CFLAGS_DEFAULT) -DMAIN $(tinfo)/captoinfo.c $(TEST_LDFLAGS)

hardscroll$x : $(serial)/hardscroll.c $(TEST_DEPS)
	 $(LIBTOOL_LINK) -o $@ $(CFLAGS_DEFAULT) -DSCROLLDEBUG $(serial)/hardscroll.c $(TEST_LDFLAGS)

hashmap$x : $(serial)/hashmap.c $(serial)/hardscroll.c $(TEST_DEPS)
	 $(LIBTOOL_LINK) -o $@ $(CFLAGS_DEFAULT) -DHASHDEBUG $(serial)/hashmap.c $(serial)/hardscroll.c $(TEST_LDFLAGS)

lib_mvcur$x : $(serial)/lib_mvcur.c $(TEST_DEPS) \
		../obj_s/dump_entry$o
	 $(LIBTOOL_LINK) -o $@ $(CFLAGS_DEFAULT) -DNCURSES_TEST -I$(serial)/../../progs $(serial)/lib_mvcur.c ../obj_s/dump_entry$o $(TEST_LDFLAGS)

link_test$x : ./link_test.c $(TEST_DEPS) \
		../obj_s/link_test$o
	 $(CC) -o $@ $(CFLAGS_DEFAULT) ../obj_s/link_test$o $(TEST_LDFLAGS)

../obj_s/dump_entry$o:
	cd ../progs && $(MAKE) ../obj_s/dump_entry$o

###############################################################################
# The remainder of this file is automatically generated during configuration
###############################################################################

# Generated by CF_LIB_RULES
resulting.map: 
	sed  -e "s/NCURSES\([WT]\+\)\?_/NCURSES\15.4_/g" -e "/deprecated in ABI5.4/d" <  >$@

clean::
	rm -f resulting.map

# generated by mk-0th.awk
#   libname:    ncursesw
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#   ticlib:     
#   termlib:    

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(serial)/hardscroll.c \
	$(serial)/hashmap.c \
	$(base)/lib_addch.c \
	$(base)/lib_addstr.c \
	$(base)/lib_beep.c \
	$(base)/lib_bkgd.c \
	$(base)/lib_box.c \
	$(base)/lib_chgat.c \
	$(base)/lib_clear.c \
	$(base)/lib_clearok.c \
	$(base)/lib_clrbot.c \
	$(base)/lib_clreol.c \
	$(base)/lib_color.c \
	$(base)/lib_colorset.c \
	$(base)/lib_delch.c \
	$(base)/lib_delwin.c \
	$(base)/lib_echo.c \
	$(base)/lib_endwin.c \
	$(base)/lib_erase.c \
	$(base)/lib_flash.c \
	./lib_gen.c \
	$(base)/lib_getch.c \
	$(base)/lib_getstr.c \
	$(base)/lib_hline.c \
	$(base)/lib_immedok.c \
	$(base)/lib_inchstr.c \
	$(base)/lib_initscr.c \
	$(base)/lib_insch.c \
	$(base)/lib_insdel.c \
	$(base)/lib_insnstr.c \
	$(base)/lib_instr.c \
	$(base)/lib_isendwin.c \
	$(base)/lib_leaveok.c \
	$(base)/lib_mouse.c \
	$(base)/lib_move.c \
	$(serial)/lib_mvcur.c \
	$(base)/lib_mvwin.c \
	$(base)/lib_newterm.c \
	$(base)/lib_newwin.c \
	$(base)/lib_nl.c \
	$(base)/lib_overlay.c \
	$(base)/lib_pad.c \
	$(base)/lib_printw.c \
	$(base)/lib_redrawln.c \
	$(base)/lib_refresh.c \
	$(base)/lib_restart.c \
	$(base)/lib_scanw.c \
	$(base)/lib_screen.c \
	$(base)/lib_scroll.c \
	$(base)/lib_scrollok.c \
	$(base)/lib_scrreg.c \
	$(base)/lib_set_term.c \
	$(base)/lib_slk.c \
	$(base)/lib_slkatr_set.c \
	$(base)/lib_slkatrof.c \
	$(base)/lib_slkatron.c \
	$(base)/lib_slkatrset.c \
	$(base)/lib_slkattr.c \
	$(base)/lib_slkclear.c \
	$(base)/lib_slkcolor.c \
	$(base)/lib_slkinit.c \
	$(base)/lib_slklab.c \
	$(base)/lib_slkrefr.c \
	$(base)/lib_slkset.c \
	$(base)/lib_slktouch.c \
	$(base)/lib_touch.c \
	$(trace)/lib_tracedmp.c \
	$(trace)/lib_tracemse.c \
	$(serial)/lib_tstp.c \
	$(base)/lib_ungetch.c \
	$(serial)/lib_vidattr.c \
	$(base)/lib_vline.c \
	$(base)/lib_wattroff.c \
	$(base)/lib_wattron.c \
	$(base)/lib_winch.c \
	$(base)/lib_window.c \
	$(base)/nc_panel.c \
	$(base)/safe_sprintf.c \
	$(serial)/tty_update.c \
	$(trace)/varargs.c \
	$(base)/vsscanf.c \
	$(base)/lib_freeall.c \
	$(wide)/charable.c \
	$(wide)/lib_add_wch.c \
	$(wide)/lib_box_set.c \
	$(wide)/lib_cchar.c \
	$(wide)/lib_erasewchar.c \
	$(wide)/lib_get_wch.c \
	$(wide)/lib_get_wstr.c \
	$(wide)/lib_hline_set.c \
	$(wide)/lib_in_wch.c \
	$(wide)/lib_in_wchnstr.c \
	$(wide)/lib_ins_wch.c \
	$(wide)/lib_inwstr.c \
	$(wide)/lib_key_name.c \
	$(wide)/lib_pecho_wchar.c \
	$(wide)/lib_slk_wset.c \
	$(wide)/lib_unget_wch.c \
	$(wide)/lib_vid_attr.c \
	$(wide)/lib_vline_set.c \
	$(wide)/lib_wacs.c \
	$(wide)/lib_wunctrl.c \
	./expanded.c \
	$(base)/legacy_coding.c \
	$(base)/lib_dft_fgbg.c \
	$(tinfo)/lib_print.c \
	$(base)/resizeterm.c \
	$(trace)/trace_xnames.c \
	$(tinfo)/use_screen.c \
	$(base)/use_window.c \
	$(base)/wresize.c \
	$(tinfo)/access.c \
	$(tinfo)/add_tries.c \
	$(tinfo)/alloc_ttype.c \
	./codes.c \
	./comp_captab.c \
	$(tinfo)/comp_error.c \
	$(tinfo)/comp_hash.c \
	$(tinfo)/db_iterator.c \
	$(tinfo)/doalloc.c \
	$(tinfo)/entries.c \
	./fallback.c \
	$(tinfo)/free_ttype.c \
	$(tinfo)/getenv_num.c \
	$(tinfo)/home_terminfo.c \
	$(tinfo)/init_keytry.c \
	$(tinfo)/lib_acs.c \
	$(tinfo)/lib_baudrate.c \
	$(tinfo)/lib_cur_term.c \
	$(tinfo)/lib_data.c \
	$(tinfo)/lib_has_cap.c \
	$(tinfo)/lib_kernel.c \
	./lib_keyname.c \
	$(tinfo)/lib_longname.c \
	$(tinfo)/lib_napms.c \
	$(tinfo)/lib_options.c \
	$(tinfo)/lib_raw.c \
	$(tinfo)/lib_setup.c \
	$(tinfo)/lib_termcap.c \
	$(tinfo)/lib_termname.c \
	$(tinfo)/lib_tgoto.c \
	$(tinfo)/lib_ti.c \
	$(tinfo)/lib_tparm.c \
	$(tinfo)/lib_tputs.c \
	$(trace)/lib_trace.c \
	$(trace)/lib_traceatr.c \
	$(trace)/lib_tracebits.c \
	$(trace)/lib_tracechr.c \
	$(tinfo)/lib_ttyflags.c \
	$(serial)/lib_twait.c \
	$(tinfo)/name_match.c \
	./names.c \
	$(tinfo)/obsolete.c \
	$(tinfo)/read_entry.c \
	$(tinfo)/read_termcap.c \
	$(tinfo)/strings.c \
	$(trace)/trace_buf.c \
	$(trace)/trace_tries.c \
	$(base)/tries.c \
	$(tinfo)/trim_sgr0.c \
	./unctrl.c \
	$(trace)/visbuf.c \
	$(tinfo)/alloc_entry.c \
	$(tinfo)/captoinfo.c \
	$(tinfo)/comp_expand.c \
	$(tinfo)/comp_parse.c \
	$(tinfo)/comp_scan.c \
	$(tinfo)/parse_entry.c \
	$(tinfo)/write_entry.c \
	$(base)/define_key.c \
	$(tinfo)/hashed_db.c \
	$(base)/key_defined.c \
	$(base)/keybound.c \
	$(base)/keyok.c \
	$(base)/version.c

# Producing llib-lncursesw is time-consuming, so there's no direct-dependency for
# it in the lintlib rule.  We'll only remove in the cleanest setup.

NCURSESW_SRC = \
	$(serial)/hardscroll.c \
	$(serial)/hashmap.c \
	$(base)/lib_addch.c \
	$(base)/lib_addstr.c \
	$(base)/lib_beep.c \
	$(base)/lib_bkgd.c \
	$(base)/lib_box.c \
	$(base)/lib_chgat.c \
	$(base)/lib_clear.c \
	$(base)/lib_clearok.c \
	$(base)/lib_clrbot.c \
	$(base)/lib_clreol.c \
	$(base)/lib_color.c \
	$(base)/lib_colorset.c \
	$(base)/lib_delch.c \
	$(base)/lib_delwin.c \
	$(base)/lib_echo.c \
	$(base)/lib_endwin.c \
	$(base)/lib_erase.c \
	$(base)/lib_flash.c \
	./lib_gen.c \
	$(base)/lib_getch.c \
	$(base)/lib_getstr.c \
	$(base)/lib_hline.c \
	$(base)/lib_immedok.c \
	$(base)/lib_inchstr.c \
	$(base)/lib_initscr.c \
	$(base)/lib_insch.c \
	$(base)/lib_insdel.c \
	$(base)/lib_insnstr.c \
	$(base)/lib_instr.c \
	$(base)/lib_isendwin.c \
	$(base)/lib_leaveok.c \
	$(base)/lib_mouse.c \
	$(base)/lib_move.c \
	$(serial)/lib_mvcur.c \
	$(base)/lib_mvwin.c \
	$(base)/lib_newterm.c \
	$(base)/lib_newwin.c \
	$(base)/lib_nl.c \
	$(base)/lib_overlay.c \
	$(base)/lib_pad.c \
	$(base)/lib_printw.c \
	$(base)/lib_redrawln.c \
	$(base)/lib_refresh.c \
	$(base)/lib_restart.c \
	$(base)/lib_scanw.c \
	$(base)/lib_screen.c \
	$(base)/lib_scroll.c \
	$(base)/lib_scrollok.c \
	$(base)/lib_scrreg.c \
	$(base)/lib_set_term.c \
	$(base)/lib_slk.c \
	$(base)/lib_slkatr_set.c \
	$(base)/lib_slkatrof.c \
	$(base)/lib_slkatron.c \
	$(base)/lib_slkatrset.c \
	$(base)/lib_slkattr.c \
	$(base)/lib_slkclear.c \
	$(base)/lib_slkcolor.c \
	$(base)/lib_slkinit.c \
	$(base)/lib_slklab.c \
	$(base)/lib_slkrefr.c \
	$(base)/lib_slkset.c \
	$(base)/lib_slktouch.c \
	$(base)/lib_touch.c \
	$(trace)/lib_tracedmp.c \
	$(trace)/lib_tracemse.c \
	$(serial)/lib_tstp.c \
	$(base)/lib_ungetch.c \
	$(serial)/lib_vidattr.c \
	$(base)/lib_vline.c \
	$(base)/lib_wattroff.c \
	$(base)/lib_wattron.c \
	$(base)/lib_winch.c \
	$(base)/lib_window.c \
	$(base)/nc_panel.c \
	$(base)/safe_sprintf.c \
	$(serial)/tty_update.c \
	$(trace)/varargs.c \
	$(base)/vsscanf.c \
	$(base)/lib_freeall.c \
	$(wide)/charable.c \
	$(wide)/lib_add_wch.c \
	$(wide)/lib_box_set.c \
	$(wide)/lib_cchar.c \
	$(wide)/lib_erasewchar.c \
	$(wide)/lib_get_wch.c \
	$(wide)/lib_get_wstr.c \
	$(wide)/lib_hline_set.c \
	$(wide)/lib_in_wch.c \
	$(wide)/lib_in_wchnstr.c \
	$(wide)/lib_ins_wch.c \
	$(wide)/lib_inwstr.c \
	$(wide)/lib_key_name.c \
	$(wide)/lib_pecho_wchar.c \
	$(wide)/lib_slk_wset.c \
	$(wide)/lib_unget_wch.c \
	$(wide)/lib_vid_attr.c \
	$(wide)/lib_vline_set.c \
	$(wide)/lib_wacs.c \
	$(wide)/lib_wunctrl.c \
	./expanded.c \
	$(base)/legacy_coding.c \
	$(base)/lib_dft_fgbg.c \
	$(tinfo)/lib_print.c \
	$(base)/resizeterm.c \
	$(trace)/trace_xnames.c \
	$(tinfo)/use_screen.c \
	$(base)/use_window.c \
	$(base)/wresize.c \
	$(tinfo)/access.c \
	$(tinfo)/add_tries.c \
	$(tinfo)/alloc_ttype.c \
	./codes.c \
	./comp_captab.c \
	$(tinfo)/comp_error.c \
	$(tinfo)/comp_hash.c \
	$(tinfo)/db_iterator.c \
	$(tinfo)/doalloc.c \
	$(tinfo)/entries.c \
	./fallback.c \
	$(tinfo)/free_ttype.c \
	$(tinfo)/getenv_num.c \
	$(tinfo)/home_terminfo.c \
	$(tinfo)/init_keytry.c \
	$(tinfo)/lib_acs.c \
	$(tinfo)/lib_baudrate.c \
	$(tinfo)/lib_cur_term.c \
	$(tinfo)/lib_data.c \
	$(tinfo)/lib_has_cap.c \
	$(tinfo)/lib_kernel.c \
	./lib_keyname.c \
	$(tinfo)/lib_longname.c \
	$(tinfo)/lib_napms.c \
	$(tinfo)/lib_options.c \
	$(tinfo)/lib_raw.c \
	$(tinfo)/lib_setup.c \
	$(tinfo)/lib_termcap.c \
	$(tinfo)/lib_termname.c \
	$(tinfo)/lib_tgoto.c \
	$(tinfo)/lib_ti.c \
	$(tinfo)/lib_tparm.c \
	$(tinfo)/lib_tputs.c \
	$(trace)/lib_trace.c \
	$(trace)/lib_traceatr.c \
	$(trace)/lib_tracebits.c \
	$(trace)/lib_tracechr.c \
	$(tinfo)/lib_ttyflags.c \
	$(serial)/lib_twait.c \
	$(tinfo)/name_match.c \
	./names.c \
	$(tinfo)/obsolete.c \
	$(tinfo)/read_entry.c \
	$(tinfo)/read_termcap.c \
	$(tinfo)/strings.c \
	$(trace)/trace_buf.c \
	$(trace)/trace_tries.c \
	$(base)/tries.c \
	$(tinfo)/trim_sgr0.c \
	./unctrl.c \
	$(trace)/visbuf.c \
	$(tinfo)/alloc_entry.c \
	$(tinfo)/captoinfo.c \
	$(tinfo)/comp_expand.c \
	$(tinfo)/comp_parse.c \
	$(tinfo)/comp_scan.c \
	$(tinfo)/parse_entry.c \
	$(tinfo)/write_entry.c \
	$(base)/define_key.c \
	$(tinfo)/hashed_db.c \
	$(base)/key_defined.c \
	$(base)/keybound.c \
	$(base)/keyok.c \
	$(base)/version.c

clean ::
	rm -f llib-lncursesw.*

realclean ::
	rm -f llib-lncursesw

llib-lncursesw : $(NCURSESW_SRC)
	cproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) $(NCURSESW_SRC) >$@

lintlib ::
	sh $(srcdir)/../misc/makellib ncursesw $(CPPFLAGS)

lint ::
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(NCURSESW_SRC) $(LINT_LIBS)

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          ncursesw
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
#  depend:        ../include/ncurses_cfg.h ./curses.priv.h
#  host:          arm-apple-darwin23.0.0

SHARED_OBJS = \
	../obj_s/hardscroll$o \
	../obj_s/hashmap$o \
	../obj_s/lib_addch$o \
	../obj_s/lib_addstr$o \
	../obj_s/lib_beep$o \
	../obj_s/lib_bkgd$o \
	../obj_s/lib_box$o \
	../obj_s/lib_chgat$o \
	../obj_s/lib_clear$o \
	../obj_s/lib_clearok$o \
	../obj_s/lib_clrbot$o \
	../obj_s/lib_clreol$o \
	../obj_s/lib_color$o \
	../obj_s/lib_colorset$o \
	../obj_s/lib_delch$o \
	../obj_s/lib_delwin$o \
	../obj_s/lib_echo$o \
	../obj_s/lib_endwin$o \
	../obj_s/lib_erase$o \
	../obj_s/lib_flash$o \
	../obj_s/lib_gen$o \
	../obj_s/lib_getch$o \
	../obj_s/lib_getstr$o \
	../obj_s/lib_hline$o \
	../obj_s/lib_immedok$o \
	../obj_s/lib_inchstr$o \
	../obj_s/lib_initscr$o \
	../obj_s/lib_insch$o \
	../obj_s/lib_insdel$o \
	../obj_s/lib_insnstr$o \
	../obj_s/lib_instr$o \
	../obj_s/lib_isendwin$o \
	../obj_s/lib_leaveok$o \
	../obj_s/lib_mouse$o \
	../obj_s/lib_move$o \
	../obj_s/lib_mvcur$o \
	../obj_s/lib_mvwin$o \
	../obj_s/lib_newterm$o \
	../obj_s/lib_newwin$o \
	../obj_s/lib_nl$o \
	../obj_s/lib_overlay$o \
	../obj_s/lib_pad$o \
	../obj_s/lib_printw$o \
	../obj_s/lib_redrawln$o \
	../obj_s/lib_refresh$o \
	../obj_s/lib_restart$o \
	../obj_s/lib_scanw$o \
	../obj_s/lib_screen$o \
	../obj_s/lib_scroll$o \
	../obj_s/lib_scrollok$o \
	../obj_s/lib_scrreg$o \
	../obj_s/lib_set_term$o \
	../obj_s/lib_slk$o \
	../obj_s/lib_slkatr_set$o \
	../obj_s/lib_slkatrof$o \
	../obj_s/lib_slkatron$o \
	../obj_s/lib_slkatrset$o \
	../obj_s/lib_slkattr$o \
	../obj_s/lib_slkclear$o \
	../obj_s/lib_slkcolor$o \
	../obj_s/lib_slkinit$o \
	../obj_s/lib_slklab$o \
	../obj_s/lib_slkrefr$o \
	../obj_s/lib_slkset$o \
	../obj_s/lib_slktouch$o \
	../obj_s/lib_touch$o \
	../obj_s/lib_tstp$o \
	../obj_s/lib_ungetch$o \
	../obj_s/lib_vidattr$o \
	../obj_s/lib_vline$o \
	../obj_s/lib_wattroff$o \
	../obj_s/lib_wattron$o \
	../obj_s/lib_winch$o \
	../obj_s/lib_window$o \
	../obj_s/nc_panel$o \
	../obj_s/safe_sprintf$o \
	../obj_s/tty_update$o \
	../obj_s/varargs$o \
	../obj_s/vsscanf$o \
	../obj_s/lib_freeall$o \
	../obj_s/charable$o \
	../obj_s/lib_add_wch$o \
	../obj_s/lib_box_set$o \
	../obj_s/lib_cchar$o \
	../obj_s/lib_erasewchar$o \
	../obj_s/lib_get_wch$o \
	../obj_s/lib_get_wstr$o \
	../obj_s/lib_hline_set$o \
	../obj_s/lib_in_wch$o \
	../obj_s/lib_in_wchnstr$o \
	../obj_s/lib_ins_wch$o \
	../obj_s/lib_inwstr$o \
	../obj_s/lib_key_name$o \
	../obj_s/lib_pecho_wchar$o \
	../obj_s/lib_slk_wset$o \
	../obj_s/lib_unget_wch$o \
	../obj_s/lib_vid_attr$o \
	../obj_s/lib_vline_set$o \
	../obj_s/lib_wacs$o \
	../obj_s/lib_wunctrl$o \
	../obj_s/expanded$o \
	../obj_s/legacy_coding$o \
	../obj_s/lib_dft_fgbg$o \
	../obj_s/lib_print$o \
	../obj_s/resizeterm$o \
	../obj_s/use_screen$o \
	../obj_s/use_window$o \
	../obj_s/wresize$o \
	../obj_s/access$o \
	../obj_s/add_tries$o \
	../obj_s/alloc_ttype$o \
	../obj_s/codes$o \
	../obj_s/comp_captab$o \
	../obj_s/comp_error$o \
	../obj_s/comp_hash$o \
	../obj_s/db_iterator$o \
	../obj_s/doalloc$o \
	../obj_s/entries$o \
	../obj_s/fallback$o \
	../obj_s/free_ttype$o \
	../obj_s/getenv_num$o \
	../obj_s/home_terminfo$o \
	../obj_s/init_keytry$o \
	../obj_s/lib_acs$o \
	../obj_s/lib_baudrate$o \
	../obj_s/lib_cur_term$o \
	../obj_s/lib_data$o \
	../obj_s/lib_has_cap$o \
	../obj_s/lib_kernel$o \
	../obj_s/lib_keyname$o \
	../obj_s/lib_longname$o \
	../obj_s/lib_napms$o \
	../obj_s/lib_options$o \
	../obj_s/lib_raw$o \
	../obj_s/lib_setup$o \
	../obj_s/lib_termcap$o \
	../obj_s/lib_termname$o \
	../obj_s/lib_tgoto$o \
	../obj_s/lib_ti$o \
	../obj_s/lib_tparm$o \
	../obj_s/lib_tputs$o \
	../obj_s/lib_trace$o \
	../obj_s/lib_ttyflags$o \
	../obj_s/lib_twait$o \
	../obj_s/name_match$o \
	../obj_s/names$o \
	../obj_s/obsolete$o \
	../obj_s/read_entry$o \
	../obj_s/read_termcap$o \
	../obj_s/strings$o \
	../obj_s/tries$o \
	../obj_s/trim_sgr0$o \
	../obj_s/unctrl$o \
	../obj_s/visbuf$o \
	../obj_s/alloc_entry$o \
	../obj_s/captoinfo$o \
	../obj_s/comp_expand$o \
	../obj_s/comp_parse$o \
	../obj_s/comp_scan$o \
	../obj_s/parse_entry$o \
	../obj_s/write_entry$o \
	../obj_s/define_key$o \
	../obj_s/hashed_db$o \
	../obj_s/key_defined$o \
	../obj_s/keybound$o \
	../obj_s/keyok$o \
	../obj_s/version$o

$(SHARED_OBJS) : ../include/ncurses_cfg.h ./curses.priv.h

../lib/libncursesw.$(ABI_VERSION).dylib : \
		../lib \
		$(RESULTING_SYMS) $(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd ../lib && ($(LN_S) libncursesw.$(ABI_VERSION).dylib libncursesw.dylib; )

install \
install.libs \
install.ncursesw :: $(DESTDIR)$(libdir)/libncursesw.$(ABI_VERSION).dylib

$(DESTDIR)$(libdir)/libncursesw.$(ABI_VERSION).dylib : \
		$(DESTDIR)$(libdir) \
		$(RESULTING_SYMS) $(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd $(DESTDIR)$(libdir) && ($(LN_S) libncursesw.$(ABI_VERSION).dylib libncursesw.dylib; )

uninstall \
uninstall.libs \
uninstall.ncursesw ::
	@echo uninstalling $(DESTDIR)$(libdir)/libncursesw.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libncursesw.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libncursesw.dylib

clean ::
	-rm -f ../lib/libncursesw.$(ABI_VERSION).dylib
	-rm -f ../lib/libncursesw.dylib

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
#   srcdir:     .

../obj_s/hardscroll$o :	$(serial)/hardscroll.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/hardscroll.c -o ../obj_s/hardscroll$o

../obj_s/hashmap$o :	$(serial)/hashmap.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/hashmap.c -o ../obj_s/hashmap$o

../obj_s/lib_addch$o :	$(base)/lib_addch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_addch.c -o ../obj_s/lib_addch$o

../obj_s/lib_addstr$o :	$(base)/lib_addstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_addstr.c -o ../obj_s/lib_addstr$o

../obj_s/lib_beep$o :	$(base)/lib_beep.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_beep.c -o ../obj_s/lib_beep$o

../obj_s/lib_bkgd$o :	$(base)/lib_bkgd.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_bkgd.c -o ../obj_s/lib_bkgd$o

../obj_s/lib_box$o :	$(base)/lib_box.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_box.c -o ../obj_s/lib_box$o

../obj_s/lib_chgat$o :	$(base)/lib_chgat.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_chgat.c -o ../obj_s/lib_chgat$o

../obj_s/lib_clear$o :	$(base)/lib_clear.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_clear.c -o ../obj_s/lib_clear$o

../obj_s/lib_clearok$o :	$(base)/lib_clearok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_clearok.c -o ../obj_s/lib_clearok$o

../obj_s/lib_clrbot$o :	$(base)/lib_clrbot.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_clrbot.c -o ../obj_s/lib_clrbot$o

../obj_s/lib_clreol$o :	$(base)/lib_clreol.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_clreol.c -o ../obj_s/lib_clreol$o

../obj_s/lib_color$o :	$(base)/lib_color.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_color.c -o ../obj_s/lib_color$o

../obj_s/lib_colorset$o :	$(base)/lib_colorset.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_colorset.c -o ../obj_s/lib_colorset$o

../obj_s/lib_delch$o :	$(base)/lib_delch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_delch.c -o ../obj_s/lib_delch$o

../obj_s/lib_delwin$o :	$(base)/lib_delwin.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_delwin.c -o ../obj_s/lib_delwin$o

../obj_s/lib_echo$o :	$(base)/lib_echo.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_echo.c -o ../obj_s/lib_echo$o

../obj_s/lib_endwin$o :	$(base)/lib_endwin.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_endwin.c -o ../obj_s/lib_endwin$o

../obj_s/lib_erase$o :	$(base)/lib_erase.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_erase.c -o ../obj_s/lib_erase$o

../obj_s/lib_flash$o :	$(base)/lib_flash.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_flash.c -o ../obj_s/lib_flash$o

../obj_s/lib_gen$o :	./lib_gen.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/lib_gen.c -o ../obj_s/lib_gen$o

../obj_s/lib_getch$o :	$(base)/lib_getch.c \
			$(HEADER_DEPS) \
			$(srcdir)/fifo_defs.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_getch.c -o ../obj_s/lib_getch$o

../obj_s/lib_getstr$o :	$(base)/lib_getstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_getstr.c -o ../obj_s/lib_getstr$o

../obj_s/lib_hline$o :	$(base)/lib_hline.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_hline.c -o ../obj_s/lib_hline$o

../obj_s/lib_immedok$o :	$(base)/lib_immedok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_immedok.c -o ../obj_s/lib_immedok$o

../obj_s/lib_inchstr$o :	$(base)/lib_inchstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_inchstr.c -o ../obj_s/lib_inchstr$o

../obj_s/lib_initscr$o :	$(base)/lib_initscr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_initscr.c -o ../obj_s/lib_initscr$o

../obj_s/lib_insch$o :	$(base)/lib_insch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_insch.c -o ../obj_s/lib_insch$o

../obj_s/lib_insdel$o :	$(base)/lib_insdel.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_insdel.c -o ../obj_s/lib_insdel$o

../obj_s/lib_insnstr$o :	$(base)/lib_insnstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_insnstr.c -o ../obj_s/lib_insnstr$o

../obj_s/lib_instr$o :	$(base)/lib_instr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_instr.c -o ../obj_s/lib_instr$o

../obj_s/lib_isendwin$o :	$(base)/lib_isendwin.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_isendwin.c -o ../obj_s/lib_isendwin$o

../obj_s/lib_leaveok$o :	$(base)/lib_leaveok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_leaveok.c -o ../obj_s/lib_leaveok$o

../obj_s/lib_mouse$o :	$(base)/lib_mouse.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_mouse.c -o ../obj_s/lib_mouse$o

../obj_s/lib_move$o :	$(base)/lib_move.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_move.c -o ../obj_s/lib_move$o

../obj_s/lib_mvcur$o :	$(serial)/lib_mvcur.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			$(srcdir)/../progs/dump_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/lib_mvcur.c -o ../obj_s/lib_mvcur$o

../obj_s/lib_mvwin$o :	$(base)/lib_mvwin.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_mvwin.c -o ../obj_s/lib_mvwin$o

../obj_s/lib_newterm$o :	$(base)/lib_newterm.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_newterm.c -o ../obj_s/lib_newterm$o

../obj_s/lib_newwin$o :	$(base)/lib_newwin.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_newwin.c -o ../obj_s/lib_newwin$o

../obj_s/lib_nl$o :	$(base)/lib_nl.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_nl.c -o ../obj_s/lib_nl$o

../obj_s/lib_overlay$o :	$(base)/lib_overlay.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_overlay.c -o ../obj_s/lib_overlay$o

../obj_s/lib_pad$o :	$(base)/lib_pad.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_pad.c -o ../obj_s/lib_pad$o

../obj_s/lib_printw$o :	$(base)/lib_printw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_printw.c -o ../obj_s/lib_printw$o

../obj_s/lib_redrawln$o :	$(base)/lib_redrawln.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_redrawln.c -o ../obj_s/lib_redrawln$o

../obj_s/lib_refresh$o :	$(base)/lib_refresh.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_refresh.c -o ../obj_s/lib_refresh$o

../obj_s/lib_restart$o :	$(base)/lib_restart.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_restart.c -o ../obj_s/lib_restart$o

../obj_s/lib_scanw$o :	$(base)/lib_scanw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_scanw.c -o ../obj_s/lib_scanw$o

../obj_s/lib_screen$o :	$(base)/lib_screen.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_screen.c -o ../obj_s/lib_screen$o

../obj_s/lib_scroll$o :	$(base)/lib_scroll.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_scroll.c -o ../obj_s/lib_scroll$o

../obj_s/lib_scrollok$o :	$(base)/lib_scrollok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_scrollok.c -o ../obj_s/lib_scrollok$o

../obj_s/lib_scrreg$o :	$(base)/lib_scrreg.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_scrreg.c -o ../obj_s/lib_scrreg$o

../obj_s/lib_set_term$o :	$(base)/lib_set_term.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_set_term.c -o ../obj_s/lib_set_term$o

../obj_s/lib_slk$o :	$(base)/lib_slk.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slk.c -o ../obj_s/lib_slk$o

../obj_s/lib_slkatr_set$o :	$(base)/lib_slkatr_set.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkatr_set.c -o ../obj_s/lib_slkatr_set$o

../obj_s/lib_slkatrof$o :	$(base)/lib_slkatrof.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkatrof.c -o ../obj_s/lib_slkatrof$o

../obj_s/lib_slkatron$o :	$(base)/lib_slkatron.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkatron.c -o ../obj_s/lib_slkatron$o

../obj_s/lib_slkatrset$o :	$(base)/lib_slkatrset.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkatrset.c -o ../obj_s/lib_slkatrset$o

../obj_s/lib_slkattr$o :	$(base)/lib_slkattr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkattr.c -o ../obj_s/lib_slkattr$o

../obj_s/lib_slkclear$o :	$(base)/lib_slkclear.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkclear.c -o ../obj_s/lib_slkclear$o

../obj_s/lib_slkcolor$o :	$(base)/lib_slkcolor.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkcolor.c -o ../obj_s/lib_slkcolor$o

../obj_s/lib_slkinit$o :	$(base)/lib_slkinit.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkinit.c -o ../obj_s/lib_slkinit$o

../obj_s/lib_slklab$o :	$(base)/lib_slklab.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slklab.c -o ../obj_s/lib_slklab$o

../obj_s/lib_slkrefr$o :	$(base)/lib_slkrefr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkrefr.c -o ../obj_s/lib_slkrefr$o

../obj_s/lib_slkset$o :	$(base)/lib_slkset.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slkset.c -o ../obj_s/lib_slkset$o

../obj_s/lib_slktouch$o :	$(base)/lib_slktouch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_slktouch.c -o ../obj_s/lib_slktouch$o

../obj_s/lib_touch$o :	$(base)/lib_touch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_touch.c -o ../obj_s/lib_touch$o

../obj_s/lib_tstp$o :	$(serial)/lib_tstp.c \
			$(HEADER_DEPS) \
			$(srcdir)/SigAction.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/lib_tstp.c -o ../obj_s/lib_tstp$o

../obj_s/lib_ungetch$o :	$(base)/lib_ungetch.c \
			$(HEADER_DEPS) \
			$(srcdir)/fifo_defs.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_ungetch.c -o ../obj_s/lib_ungetch$o

../obj_s/lib_vidattr$o :	$(serial)/lib_vidattr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/lib_vidattr.c -o ../obj_s/lib_vidattr$o

../obj_s/lib_vline$o :	$(base)/lib_vline.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_vline.c -o ../obj_s/lib_vline$o

../obj_s/lib_wattroff$o :	$(base)/lib_wattroff.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_wattroff.c -o ../obj_s/lib_wattroff$o

../obj_s/lib_wattron$o :	$(base)/lib_wattron.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_wattron.c -o ../obj_s/lib_wattron$o

../obj_s/lib_winch$o :	$(base)/lib_winch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_winch.c -o ../obj_s/lib_winch$o

../obj_s/lib_window$o :	$(base)/lib_window.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_window.c -o ../obj_s/lib_window$o

../obj_s/link_test$o :	./link_test.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/link_test.c -o ../obj_s/link_test$o

../obj_s/nc_panel$o :	$(base)/nc_panel.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/nc_panel.c -o ../obj_s/nc_panel$o

../obj_s/safe_sprintf$o :	$(base)/safe_sprintf.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/safe_sprintf.c -o ../obj_s/safe_sprintf$o

../obj_s/tty_update$o :	$(serial)/tty_update.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/tty_update.c -o ../obj_s/tty_update$o

../obj_s/varargs$o :	$(trace)/varargs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(trace)/varargs.c -o ../obj_s/varargs$o

../obj_s/vsscanf$o :	$(base)/vsscanf.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/vsscanf.c -o ../obj_s/vsscanf$o

../obj_s/lib_freeall$o :	$(base)/lib_freeall.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_freeall.c -o ../obj_s/lib_freeall$o

../obj_s/charable$o :	$(wide)/charable.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/charable.c -o ../obj_s/charable$o

../obj_s/lib_add_wch$o :	$(wide)/lib_add_wch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_add_wch.c -o ../obj_s/lib_add_wch$o

../obj_s/lib_box_set$o :	$(wide)/lib_box_set.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_box_set.c -o ../obj_s/lib_box_set$o

../obj_s/lib_cchar$o :	$(wide)/lib_cchar.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_cchar.c -o ../obj_s/lib_cchar$o

../obj_s/lib_erasewchar$o :	$(wide)/lib_erasewchar.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_erasewchar.c -o ../obj_s/lib_erasewchar$o

../obj_s/lib_get_wch$o :	$(wide)/lib_get_wch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_get_wch.c -o ../obj_s/lib_get_wch$o

../obj_s/lib_get_wstr$o :	$(wide)/lib_get_wstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_get_wstr.c -o ../obj_s/lib_get_wstr$o

../obj_s/lib_hline_set$o :	$(wide)/lib_hline_set.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_hline_set.c -o ../obj_s/lib_hline_set$o

../obj_s/lib_in_wch$o :	$(wide)/lib_in_wch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_in_wch.c -o ../obj_s/lib_in_wch$o

../obj_s/lib_in_wchnstr$o :	$(wide)/lib_in_wchnstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_in_wchnstr.c -o ../obj_s/lib_in_wchnstr$o

../obj_s/lib_ins_wch$o :	$(wide)/lib_ins_wch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_ins_wch.c -o ../obj_s/lib_ins_wch$o

../obj_s/lib_inwstr$o :	$(wide)/lib_inwstr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_inwstr.c -o ../obj_s/lib_inwstr$o

../obj_s/lib_key_name$o :	$(wide)/lib_key_name.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_key_name.c -o ../obj_s/lib_key_name$o

../obj_s/lib_pecho_wchar$o :	$(wide)/lib_pecho_wchar.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_pecho_wchar.c -o ../obj_s/lib_pecho_wchar$o

../obj_s/lib_slk_wset$o :	$(wide)/lib_slk_wset.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_slk_wset.c -o ../obj_s/lib_slk_wset$o

../obj_s/lib_unget_wch$o :	$(wide)/lib_unget_wch.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_unget_wch.c -o ../obj_s/lib_unget_wch$o

../obj_s/lib_vid_attr$o :	$(wide)/lib_vid_attr.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_vid_attr.c -o ../obj_s/lib_vid_attr$o

../obj_s/lib_vline_set$o :	$(wide)/lib_vline_set.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_vline_set.c -o ../obj_s/lib_vline_set$o

../obj_s/lib_wacs$o :	$(wide)/lib_wacs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_wacs.c -o ../obj_s/lib_wacs$o

../obj_s/lib_wunctrl$o :	$(wide)/lib_wunctrl.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(wide)/lib_wunctrl.c -o ../obj_s/lib_wunctrl$o

../obj_s/expanded$o :	./expanded.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/expanded.c -o ../obj_s/expanded$o

../obj_s/legacy_coding$o :	$(base)/legacy_coding.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/legacy_coding.c -o ../obj_s/legacy_coding$o

../obj_s/lib_dft_fgbg$o :	$(base)/lib_dft_fgbg.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/lib_dft_fgbg.c -o ../obj_s/lib_dft_fgbg$o

../obj_s/lib_print$o :	$(tinfo)/lib_print.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_print.c -o ../obj_s/lib_print$o

../obj_s/resizeterm$o :	$(base)/resizeterm.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/resizeterm.c -o ../obj_s/resizeterm$o

../obj_s/use_screen$o :	$(tinfo)/use_screen.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/use_screen.c -o ../obj_s/use_screen$o

../obj_s/use_window$o :	$(base)/use_window.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/use_window.c -o ../obj_s/use_window$o

../obj_s/wresize$o :	$(base)/wresize.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/wresize.c -o ../obj_s/wresize$o

../obj_s/access$o :	$(tinfo)/access.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/access.c -o ../obj_s/access$o

../obj_s/add_tries$o :	$(tinfo)/add_tries.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/add_tries.c -o ../obj_s/add_tries$o

../obj_s/alloc_ttype$o :	$(tinfo)/alloc_ttype.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/alloc_ttype.c -o ../obj_s/alloc_ttype$o

../obj_s/codes$o :	./codes.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/codes.c -o ../obj_s/codes$o

../obj_s/comp_captab$o :	./comp_captab.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			../include/hashsize.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/comp_captab.c -o ../obj_s/comp_captab$o

../obj_s/comp_error$o :	$(tinfo)/comp_error.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/comp_error.c -o ../obj_s/comp_error$o

../obj_s/comp_hash$o :	$(tinfo)/comp_hash.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			../include/hashsize.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/comp_hash.c -o ../obj_s/comp_hash$o

../obj_s/db_iterator$o :	$(tinfo)/db_iterator.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/db_iterator.c -o ../obj_s/db_iterator$o

../obj_s/doalloc$o :	$(tinfo)/doalloc.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/doalloc.c -o ../obj_s/doalloc$o

../obj_s/entries$o :	$(tinfo)/entries.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/entries.c -o ../obj_s/entries$o

../obj_s/fallback$o :	./fallback.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/fallback.c -o ../obj_s/fallback$o

../obj_s/free_ttype$o :	$(tinfo)/free_ttype.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/free_ttype.c -o ../obj_s/free_ttype$o

../obj_s/getenv_num$o :	$(tinfo)/getenv_num.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/getenv_num.c -o ../obj_s/getenv_num$o

../obj_s/home_terminfo$o :	$(tinfo)/home_terminfo.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/home_terminfo.c -o ../obj_s/home_terminfo$o

../obj_s/init_keytry$o :	$(tinfo)/init_keytry.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			init_keytry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/init_keytry.c -o ../obj_s/init_keytry$o

../obj_s/lib_acs$o :	$(tinfo)/lib_acs.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_acs.c -o ../obj_s/lib_acs$o

../obj_s/lib_baudrate$o :	$(tinfo)/lib_baudrate.c \
			$(HEADER_DEPS) \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_baudrate.c -o ../obj_s/lib_baudrate$o

../obj_s/lib_cur_term$o :	$(tinfo)/lib_cur_term.c \
			$(HEADER_DEPS) \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_cur_term.c -o ../obj_s/lib_cur_term$o

../obj_s/lib_data$o :	$(tinfo)/lib_data.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_data.c -o ../obj_s/lib_data$o

../obj_s/lib_has_cap$o :	$(tinfo)/lib_has_cap.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_has_cap.c -o ../obj_s/lib_has_cap$o

../obj_s/lib_kernel$o :	$(tinfo)/lib_kernel.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_kernel.c -o ../obj_s/lib_kernel$o

../obj_s/lib_keyname$o :	./lib_keyname.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/lib_keyname.c -o ../obj_s/lib_keyname$o

../obj_s/lib_longname$o :	$(tinfo)/lib_longname.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_longname.c -o ../obj_s/lib_longname$o

../obj_s/lib_napms$o :	$(tinfo)/lib_napms.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_napms.c -o ../obj_s/lib_napms$o

../obj_s/lib_options$o :	$(tinfo)/lib_options.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_options.c -o ../obj_s/lib_options$o

../obj_s/lib_raw$o :	$(tinfo)/lib_raw.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_raw.c -o ../obj_s/lib_raw$o

../obj_s/lib_setup$o :	$(tinfo)/lib_setup.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_setup.c -o ../obj_s/lib_setup$o

../obj_s/lib_termcap$o :	$(tinfo)/lib_termcap.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			../include/termcap.h \
			$(INCDIR)/capdefaults.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_termcap.c -o ../obj_s/lib_termcap$o

../obj_s/lib_termname$o :	$(tinfo)/lib_termname.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_termname.c -o ../obj_s/lib_termname$o

../obj_s/lib_tgoto$o :	$(tinfo)/lib_tgoto.c \
			$(HEADER_DEPS) \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_tgoto.c -o ../obj_s/lib_tgoto$o

../obj_s/lib_ti$o :	$(tinfo)/lib_ti.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_ti.c -o ../obj_s/lib_ti$o

../obj_s/lib_tparm$o :	$(tinfo)/lib_tparm.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_tparm.c -o ../obj_s/lib_tparm$o

../obj_s/lib_tputs$o :	$(tinfo)/lib_tputs.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			../include/termcap.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_tputs.c -o ../obj_s/lib_tputs$o

../obj_s/lib_trace$o :	$(trace)/lib_trace.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(trace)/lib_trace.c -o ../obj_s/lib_trace$o

../obj_s/lib_ttyflags$o :	$(tinfo)/lib_ttyflags.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/lib_ttyflags.c -o ../obj_s/lib_ttyflags$o

../obj_s/lib_twait$o :	$(serial)/lib_twait.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(serial)/lib_twait.c -o ../obj_s/lib_twait$o

../obj_s/name_match$o :	$(tinfo)/name_match.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/name_match.c -o ../obj_s/name_match$o

../obj_s/names$o :	./names.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/names.c -o ../obj_s/names$o

../obj_s/obsolete$o :	$(tinfo)/obsolete.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/obsolete.c -o ../obj_s/obsolete$o

../obj_s/read_entry$o :	$(tinfo)/read_entry.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			$(INCDIR)/hashed_db.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/read_entry.c -o ../obj_s/read_entry$o

../obj_s/read_termcap$o :	$(tinfo)/read_termcap.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/read_termcap.c -o ../obj_s/read_termcap$o

../obj_s/strings$o :	$(tinfo)/strings.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/strings.c -o ../obj_s/strings$o

../obj_s/tries$o :	$(base)/tries.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/tries.c -o ../obj_s/tries$o

../obj_s/trim_sgr0$o :	$(tinfo)/trim_sgr0.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/trim_sgr0.c -o ../obj_s/trim_sgr0$o

../obj_s/unctrl$o :	./unctrl.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/unctrl.c -o ../obj_s/unctrl$o

../obj_s/visbuf$o :	$(trace)/visbuf.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(trace)/visbuf.c -o ../obj_s/visbuf$o

../obj_s/alloc_entry$o :	$(tinfo)/alloc_entry.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/alloc_entry.c -o ../obj_s/alloc_entry$o

../obj_s/captoinfo$o :	$(tinfo)/captoinfo.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/captoinfo.c -o ../obj_s/captoinfo$o

../obj_s/comp_expand$o :	$(tinfo)/comp_expand.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/comp_expand.c -o ../obj_s/comp_expand$o

../obj_s/comp_parse$o :	$(tinfo)/comp_parse.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/comp_parse.c -o ../obj_s/comp_parse$o

../obj_s/comp_scan$o :	$(tinfo)/comp_scan.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/comp_scan.c -o ../obj_s/comp_scan$o

../obj_s/parse_entry$o :	$(tinfo)/parse_entry.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			../include/parametrized.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/parse_entry.c -o ../obj_s/parse_entry$o

../obj_s/write_entry$o :	$(tinfo)/write_entry.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			$(INCDIR)/hashed_db.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/write_entry.c -o ../obj_s/write_entry$o

../obj_s/define_key$o :	$(base)/define_key.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/define_key.c -o ../obj_s/define_key$o

../obj_s/hashed_db$o :	$(tinfo)/hashed_db.c \
			$(HEADER_DEPS) \
			$(INCDIR)/tic.h \
			$(INCDIR)/hashed_db.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(tinfo)/hashed_db.c -o ../obj_s/hashed_db$o

../obj_s/key_defined$o :	$(base)/key_defined.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/key_defined.c -o ../obj_s/key_defined$o

../obj_s/keybound$o :	$(base)/keybound.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/keybound.c -o ../obj_s/keybound$o

../obj_s/keyok$o :	$(base)/keyok.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/keyok.c -o ../obj_s/keyok$o

../obj_s/version$o :	$(base)/version.c \
			$(HEADER_DEPS)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/$(base)/version.c -o ../obj_s/version$o

depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
