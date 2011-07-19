# $Id: Makefile.in,v 1.113 2008/10/18 14:11:54 tom Exp $
##############################################################################
# Copyright (c) 1998-2007,2008 Free Software Foundation, Inc.                #
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
top_srcdir	= /tmp/ncurses-27.roots/ncurses-27/ncurses
srcdir		= /tmp/ncurses-27.roots/ncurses-27/ncurses/ncurses
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

INCDIR		= $(srcdir)/../include
CPPFLAGS	= -DHAVE_CONFIG_H -I../ncurses -I$(srcdir) -I${srcdir}/../include -I. -I../include  -U_XOPEN_SOURCE -D_XOPEN_SOURCE=500 -DSIGWINCH=28 -D_XOPEN_SOURCE_EXTENDED -DNDEBUG

CCFLAGS		= $(CPPFLAGS) $(CFLAGS)

BUILD_CPPFLAGS	= -I../include ${CPPFLAGS}
BUILD_CC	= ${CC}
BUILD_CCFLAGS	= -DHAVE_CONFIG_H -I../ncurses -I$(srcdir) -I$(INCDIR) $(BUILD_CPPFLAGS) ${CFLAGS}
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
LDFLAGS		= -arch i386 -arch x86_64 -arch ppc             -Wl,-search_paths_first  

SHLIB_DIRS	= -L../lib
SHLIB_LIST	= $(SHLIB_DIRS) 
TINFO_LIST	= $(SHLIB_DIRS) 
TICS_LIST	= $(SHLIB_DIRS) 

MK_SHARED_LIB	= ${CC} ${CFLAGS} -dynamiclib -install_name ${libdir}/`basename $@` -compatibility_version ${ABI_VERSION} -current_version ${ABI_VERSION} -o $@

NCURSES_MAJOR	= 5
NCURSES_MINOR	= 7
REL_VERSION	= 5.7
ABI_VERSION	= 5.4

RANLIB		= ranlib

LIBRARIES	=  ../lib/libncurses.${ABI_VERSION}.dylib

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lncurses 

FALLBACK_LIST	= 

USE_BIG_STRINGS	= 1
TERMINFO_CAPS	= $(top_srcdir)/include/Caps

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

TEST_DEPS	= ../lib/libncurses.dylib
TEST_ARGS	=  -L../lib -lform -lmenu -lpanel -lncurses   
TEST_LDFLAGS	=  $(TEST_ARGS)   -arch i386 -arch x86_64 -arch ppc             -Wl,-search_paths_first

TEST_PROGS = \
	captoinfo$x \
	hardscroll$x \
	link_test$x \
	hashmap$x \
	lib_mvcur$x

base	= $(srcdir)/base
serial	= $(srcdir)/tty
tinfo	= $(srcdir)/tinfo
trace	= $(srcdir)/trace
wide	= $(srcdir)/widechar

################################################################################
all \
libs ::		$(AUTO_SRC) ../lib $(LIBRARIES)

sources:	$(AUTO_SRC)

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) :
	sh $(srcdir)/../mkdirs.sh $@

../lib : ; mkdir $@

./fallback.c : $(tinfo)/MKfallback.sh
	sh -e $(tinfo)/MKfallback.sh /usr/share/terminfo ${top_srcdir}/misc/terminfo.src $(FALLBACK_LIST) >$@

./lib_gen.c : $(base)/MKlib_gen.sh ../include/curses.h
	sh -e $(base)/MKlib_gen.sh "$(CPP) $(CPPFLAGS)" "$(AWK)" generated <../include/curses.h >$@

init_keytry.h: make_keys$(BUILD_EXEEXT) keys.list
	./make_keys$(BUILD_EXEEXT) keys.list > $@

keys.list :	$(tinfo)/MKkeys_list.sh
	AWK=$(AWK) sh $(tinfo)/MKkeys_list.sh $(TERMINFO_CAPS) | sort >$@

make_keys$(BUILD_EXEEXT) : \
		$(tinfo)/make_keys.c \
		names.c
	$(BUILD_CC) -o $@ $(BUILD_CCFLAGS) $(tinfo)/make_keys.c $(BUILD_LDFLAGS) $(BUILD_LIBS)

make_hash$(BUILD_EXEEXT) : \
		$(tinfo)/comp_hash.c \
		../include/hashsize.h
	$(BUILD_CC) -o $@ $(BUILD_CCFLAGS) -DMAIN_PROGRAM $(tinfo)/comp_hash.c $(BUILD_LDFLAGS) $(BUILD_LIBS)

./expanded.c : $(serial)/MKexpanded.sh
	sh -e $(serial)/MKexpanded.sh "$(CPP)" $(CPPFLAGS) > $@

./comp_captab.c: \
		make_hash$(BUILD_EXEEXT) \
		../include/hashsize.h \
		$(tinfo)/MKcaptab.sh \
		$(tinfo)/MKcaptab.awk
	sh -e $(tinfo)/MKcaptab.sh $(AWK) $(USE_BIG_STRINGS) $(tinfo)/MKcaptab.awk $(srcdir)/../include/Caps > $@

./lib_keyname.c: keys.list $(base)/MKkeyname.awk
	$(AWK) -f $(base)/MKkeyname.awk bigstrings=$(USE_BIG_STRINGS) keys.list > $@

./codes.c: $(tinfo)/MKcodes.awk
	$(AWK) -f $(tinfo)/MKcodes.awk bigstrings=$(USE_BIG_STRINGS) $(srcdir)/../include/Caps >$@

./names.c: $(tinfo)/MKnames.awk
	$(AWK) -f $(tinfo)/MKnames.awk bigstrings=$(USE_BIG_STRINGS) $(srcdir)/../include/Caps >$@

./unctrl.c: $(base)/MKunctrl.awk
	echo | $(AWK) -f $(base)/MKunctrl.awk bigstrings=$(USE_BIG_STRINGS) >$@

tags:
	ctags *.[ch] */*.[ch]

#TAGS:
#	etags *.[ch] */*.[ch]

mostlyclean ::
	-rm -f core tags TAGS *~ *.bak *.i *.ln *.atac trace
	-rm -f $(TEST_PROGS)

clean :: mostlyclean
	-rm -f $(AUTO_SRC)
	-rm -f make_keys$(BUILD_EXEEXT)
	-rm -f make_hash$(BUILD_EXEEXT)
	-rm -rf .libs

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
	sh -e $(base)/MKlib_gen.sh "$(CPP) $(CPPFLAGS)" "$(AWK)" implemented <../include/curses.h >$@

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

# generated by mk-0th.awk
#   libname:    ncurses
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs

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
	$(base)/memmove.c \
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
	$(tinfo)/comp_error.c \
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
	$(tinfo)/read_entry.c \
	$(tinfo)/read_termcap.c \
	$(tinfo)/setbuf.c \
	$(tinfo)/strings.c \
	$(trace)/trace_buf.c \
	$(trace)/trace_tries.c \
	$(base)/tries.c \
	$(tinfo)/trim_sgr0.c \
	./unctrl.c \
	$(trace)/visbuf.c \
	$(tinfo)/alloc_entry.c \
	$(tinfo)/captoinfo.c \
	./comp_captab.c \
	$(tinfo)/comp_expand.c \
	$(tinfo)/comp_hash.c \
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

# Producing llib-lncurses is time-consuming, so there's no direct-dependency for
# it in the lintlib rule.  We'll only remove in the cleanest setup.
clean ::
	rm -f llib-lncurses.*

realclean ::
	rm -f llib-lncurses

llib-lncurses : $(C_SRC)
	cproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) $(C_SRC) >$@

lintlib :
	sh $(srcdir)/../misc/makellib ncurses $(CPPFLAGS)
lint :
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(C_SRC) $(LINT_LIBS)

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          ncurses
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
#  depend:        ../include/ncurses_cfg.h ${srcdir}/curses.priv.h
#  host:          i386-apple-darwin10.0

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
	../obj_s/memmove$o \
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
	../obj_s/comp_error$o \
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
	../obj_s/read_entry$o \
	../obj_s/read_termcap$o \
	../obj_s/setbuf$o \
	../obj_s/strings$o \
	../obj_s/tries$o \
	../obj_s/trim_sgr0$o \
	../obj_s/unctrl$o \
	../obj_s/visbuf$o \
	../obj_s/alloc_entry$o \
	../obj_s/captoinfo$o \
	../obj_s/comp_captab$o \
	../obj_s/comp_expand$o \
	../obj_s/comp_hash$o \
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

$(SHARED_OBJS) : ../include/ncurses_cfg.h ${srcdir}/curses.priv.h

../lib/libncurses.$(ABI_VERSION).dylib : \
		../lib \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd ../lib && (rm -f libncurses.dylib; $(LN_S) libncurses.$(ABI_VERSION).dylib libncurses.dylib; )

install \
install.libs \
install.ncurses :: $(DESTDIR)$(libdir)/libncurses.$(ABI_VERSION).dylib

$(DESTDIR)$(libdir)/libncurses.$(ABI_VERSION).dylib : \
		$(DESTDIR)$(libdir) \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd $(DESTDIR)$(libdir) && (rm -f libncurses.dylib; $(LN_S) libncurses.$(ABI_VERSION).dylib libncurses.dylib; )
	@echo linking libncurses.$(ABI_VERSION).dylib to libcurses.dylib
	cd $(DESTDIR)$(libdir) && (rm -f libcurses.dylib; $(LN_S) libncurses.$(ABI_VERSION).dylib libcurses.dylib; )

uninstall \
uninstall.libs \
uninstall.ncurses ::
	@echo uninstalling $(DESTDIR)$(libdir)/libncurses.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libncurses.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libncurses.dylib
	-@rm -f $(DESTDIR)$(libdir)/libcurses.dylib

clean ::
	-rm -f ../lib/libncurses.$(ABI_VERSION).dylib
	-rm -f ../lib/libncurses.dylib

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

../obj_s/hardscroll$o :	$(serial)/hardscroll.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/hardscroll.c -o ../obj_s/hardscroll$o

../obj_s/hashmap$o :	$(serial)/hashmap.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/hashmap.c -o ../obj_s/hashmap$o

../obj_s/lib_addch$o :	$(base)/lib_addch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_addch.c -o ../obj_s/lib_addch$o

../obj_s/lib_addstr$o :	$(base)/lib_addstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_addstr.c -o ../obj_s/lib_addstr$o

../obj_s/lib_beep$o :	$(base)/lib_beep.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_beep.c -o ../obj_s/lib_beep$o

../obj_s/lib_bkgd$o :	$(base)/lib_bkgd.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_bkgd.c -o ../obj_s/lib_bkgd$o

../obj_s/lib_box$o :	$(base)/lib_box.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_box.c -o ../obj_s/lib_box$o

../obj_s/lib_chgat$o :	$(base)/lib_chgat.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_chgat.c -o ../obj_s/lib_chgat$o

../obj_s/lib_clear$o :	$(base)/lib_clear.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_clear.c -o ../obj_s/lib_clear$o

../obj_s/lib_clearok$o :	$(base)/lib_clearok.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_clearok.c -o ../obj_s/lib_clearok$o

../obj_s/lib_clrbot$o :	$(base)/lib_clrbot.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_clrbot.c -o ../obj_s/lib_clrbot$o

../obj_s/lib_clreol$o :	$(base)/lib_clreol.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_clreol.c -o ../obj_s/lib_clreol$o

../obj_s/lib_color$o :	$(base)/lib_color.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_color.c -o ../obj_s/lib_color$o

../obj_s/lib_colorset$o :	$(base)/lib_colorset.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_colorset.c -o ../obj_s/lib_colorset$o

../obj_s/lib_delch$o :	$(base)/lib_delch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_delch.c -o ../obj_s/lib_delch$o

../obj_s/lib_delwin$o :	$(base)/lib_delwin.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_delwin.c -o ../obj_s/lib_delwin$o

../obj_s/lib_echo$o :	$(base)/lib_echo.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_echo.c -o ../obj_s/lib_echo$o

../obj_s/lib_endwin$o :	$(base)/lib_endwin.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_endwin.c -o ../obj_s/lib_endwin$o

../obj_s/lib_erase$o :	$(base)/lib_erase.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_erase.c -o ../obj_s/lib_erase$o

../obj_s/lib_flash$o :	$(base)/lib_flash.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_flash.c -o ../obj_s/lib_flash$o

../obj_s/lib_gen$o :	./lib_gen.c \
			../include/curses.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/lib_gen.c -o ../obj_s/lib_gen$o

../obj_s/lib_getch$o :	$(base)/lib_getch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_getch.c -o ../obj_s/lib_getch$o

../obj_s/lib_getstr$o :	$(base)/lib_getstr.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_getstr.c -o ../obj_s/lib_getstr$o

../obj_s/lib_hline$o :	$(base)/lib_hline.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_hline.c -o ../obj_s/lib_hline$o

../obj_s/lib_immedok$o :	$(base)/lib_immedok.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_immedok.c -o ../obj_s/lib_immedok$o

../obj_s/lib_inchstr$o :	$(base)/lib_inchstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_inchstr.c -o ../obj_s/lib_inchstr$o

../obj_s/lib_initscr$o :	$(base)/lib_initscr.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_initscr.c -o ../obj_s/lib_initscr$o

../obj_s/lib_insch$o :	$(base)/lib_insch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_insch.c -o ../obj_s/lib_insch$o

../obj_s/lib_insdel$o :	$(base)/lib_insdel.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_insdel.c -o ../obj_s/lib_insdel$o

../obj_s/lib_insnstr$o :	$(base)/lib_insnstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_insnstr.c -o ../obj_s/lib_insnstr$o

../obj_s/lib_instr$o :	$(base)/lib_instr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_instr.c -o ../obj_s/lib_instr$o

../obj_s/lib_isendwin$o :	$(base)/lib_isendwin.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_isendwin.c -o ../obj_s/lib_isendwin$o

../obj_s/lib_leaveok$o :	$(base)/lib_leaveok.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_leaveok.c -o ../obj_s/lib_leaveok$o

../obj_s/lib_mouse$o :	$(base)/lib_mouse.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_mouse.c -o ../obj_s/lib_mouse$o

../obj_s/lib_move$o :	$(base)/lib_move.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_move.c -o ../obj_s/lib_move$o

../obj_s/lib_mvcur$o :	$(serial)/lib_mvcur.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/lib_mvcur.c -o ../obj_s/lib_mvcur$o

../obj_s/lib_mvwin$o :	$(base)/lib_mvwin.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_mvwin.c -o ../obj_s/lib_mvwin$o

../obj_s/lib_newterm$o :	$(base)/lib_newterm.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_newterm.c -o ../obj_s/lib_newterm$o

../obj_s/lib_newwin$o :	$(base)/lib_newwin.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_newwin.c -o ../obj_s/lib_newwin$o

../obj_s/lib_nl$o :	$(base)/lib_nl.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_nl.c -o ../obj_s/lib_nl$o

../obj_s/lib_overlay$o :	$(base)/lib_overlay.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_overlay.c -o ../obj_s/lib_overlay$o

../obj_s/lib_pad$o :	$(base)/lib_pad.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_pad.c -o ../obj_s/lib_pad$o

../obj_s/lib_printw$o :	$(base)/lib_printw.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_printw.c -o ../obj_s/lib_printw$o

../obj_s/lib_redrawln$o :	$(base)/lib_redrawln.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_redrawln.c -o ../obj_s/lib_redrawln$o

../obj_s/lib_refresh$o :	$(base)/lib_refresh.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_refresh.c -o ../obj_s/lib_refresh$o

../obj_s/lib_restart$o :	$(base)/lib_restart.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_restart.c -o ../obj_s/lib_restart$o

../obj_s/lib_scanw$o :	$(base)/lib_scanw.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_scanw.c -o ../obj_s/lib_scanw$o

../obj_s/lib_screen$o :	$(base)/lib_screen.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_screen.c -o ../obj_s/lib_screen$o

../obj_s/lib_scroll$o :	$(base)/lib_scroll.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_scroll.c -o ../obj_s/lib_scroll$o

../obj_s/lib_scrollok$o :	$(base)/lib_scrollok.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_scrollok.c -o ../obj_s/lib_scrollok$o

../obj_s/lib_scrreg$o :	$(base)/lib_scrreg.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_scrreg.c -o ../obj_s/lib_scrreg$o

../obj_s/lib_set_term$o :	$(base)/lib_set_term.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_set_term.c -o ../obj_s/lib_set_term$o

../obj_s/lib_slk$o :	$(base)/lib_slk.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slk.c -o ../obj_s/lib_slk$o

../obj_s/lib_slkatr_set$o :	$(base)/lib_slkatr_set.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkatr_set.c -o ../obj_s/lib_slkatr_set$o

../obj_s/lib_slkatrof$o :	$(base)/lib_slkatrof.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkatrof.c -o ../obj_s/lib_slkatrof$o

../obj_s/lib_slkatron$o :	$(base)/lib_slkatron.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkatron.c -o ../obj_s/lib_slkatron$o

../obj_s/lib_slkatrset$o :	$(base)/lib_slkatrset.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkatrset.c -o ../obj_s/lib_slkatrset$o

../obj_s/lib_slkattr$o :	$(base)/lib_slkattr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkattr.c -o ../obj_s/lib_slkattr$o

../obj_s/lib_slkclear$o :	$(base)/lib_slkclear.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkclear.c -o ../obj_s/lib_slkclear$o

../obj_s/lib_slkcolor$o :	$(base)/lib_slkcolor.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkcolor.c -o ../obj_s/lib_slkcolor$o

../obj_s/lib_slkinit$o :	$(base)/lib_slkinit.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkinit.c -o ../obj_s/lib_slkinit$o

../obj_s/lib_slklab$o :	$(base)/lib_slklab.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slklab.c -o ../obj_s/lib_slklab$o

../obj_s/lib_slkrefr$o :	$(base)/lib_slkrefr.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkrefr.c -o ../obj_s/lib_slkrefr$o

../obj_s/lib_slkset$o :	$(base)/lib_slkset.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slkset.c -o ../obj_s/lib_slkset$o

../obj_s/lib_slktouch$o :	$(base)/lib_slktouch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_slktouch.c -o ../obj_s/lib_slktouch$o

../obj_s/lib_touch$o :	$(base)/lib_touch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_touch.c -o ../obj_s/lib_touch$o

../obj_s/lib_tstp$o :	$(serial)/lib_tstp.c \
			$(srcdir)/SigAction.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/lib_tstp.c -o ../obj_s/lib_tstp$o

../obj_s/lib_ungetch$o :	$(base)/lib_ungetch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_ungetch.c -o ../obj_s/lib_ungetch$o

../obj_s/lib_vidattr$o :	$(serial)/lib_vidattr.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/lib_vidattr.c -o ../obj_s/lib_vidattr$o

../obj_s/lib_vline$o :	$(base)/lib_vline.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_vline.c -o ../obj_s/lib_vline$o

../obj_s/lib_wattroff$o :	$(base)/lib_wattroff.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_wattroff.c -o ../obj_s/lib_wattroff$o

../obj_s/lib_wattron$o :	$(base)/lib_wattron.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_wattron.c -o ../obj_s/lib_wattron$o

../obj_s/lib_winch$o :	$(base)/lib_winch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_winch.c -o ../obj_s/lib_winch$o

../obj_s/lib_window$o :	$(base)/lib_window.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_window.c -o ../obj_s/lib_window$o

../obj_s/link_test$o :	./link_test.c \
			../include/curses.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/link_test.c -o ../obj_s/link_test$o

../obj_s/nc_panel$o :	$(base)/nc_panel.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/nc_panel.c -o ../obj_s/nc_panel$o

../obj_s/safe_sprintf$o :	$(base)/safe_sprintf.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/safe_sprintf.c -o ../obj_s/safe_sprintf$o

../obj_s/tty_update$o :	$(serial)/tty_update.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/tty_update.c -o ../obj_s/tty_update$o

../obj_s/varargs$o :	$(trace)/varargs.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(trace)/varargs.c -o ../obj_s/varargs$o

../obj_s/memmove$o :	$(base)/memmove.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/memmove.c -o ../obj_s/memmove$o

../obj_s/vsscanf$o :	$(base)/vsscanf.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/vsscanf.c -o ../obj_s/vsscanf$o

../obj_s/lib_freeall$o :	$(base)/lib_freeall.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_freeall.c -o ../obj_s/lib_freeall$o

../obj_s/charable$o :	$(wide)/charable.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/charable.c -o ../obj_s/charable$o

../obj_s/lib_add_wch$o :	$(wide)/lib_add_wch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_add_wch.c -o ../obj_s/lib_add_wch$o

../obj_s/lib_box_set$o :	$(wide)/lib_box_set.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_box_set.c -o ../obj_s/lib_box_set$o

../obj_s/lib_cchar$o :	$(wide)/lib_cchar.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_cchar.c -o ../obj_s/lib_cchar$o

../obj_s/lib_erasewchar$o :	$(wide)/lib_erasewchar.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_erasewchar.c -o ../obj_s/lib_erasewchar$o

../obj_s/lib_get_wch$o :	$(wide)/lib_get_wch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_get_wch.c -o ../obj_s/lib_get_wch$o

../obj_s/lib_get_wstr$o :	$(wide)/lib_get_wstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_get_wstr.c -o ../obj_s/lib_get_wstr$o

../obj_s/lib_hline_set$o :	$(wide)/lib_hline_set.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_hline_set.c -o ../obj_s/lib_hline_set$o

../obj_s/lib_in_wch$o :	$(wide)/lib_in_wch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_in_wch.c -o ../obj_s/lib_in_wch$o

../obj_s/lib_in_wchnstr$o :	$(wide)/lib_in_wchnstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_in_wchnstr.c -o ../obj_s/lib_in_wchnstr$o

../obj_s/lib_ins_wch$o :	$(wide)/lib_ins_wch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_ins_wch.c -o ../obj_s/lib_ins_wch$o

../obj_s/lib_inwstr$o :	$(wide)/lib_inwstr.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_inwstr.c -o ../obj_s/lib_inwstr$o

../obj_s/lib_key_name$o :	$(wide)/lib_key_name.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_key_name.c -o ../obj_s/lib_key_name$o

../obj_s/lib_pecho_wchar$o :	$(wide)/lib_pecho_wchar.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_pecho_wchar.c -o ../obj_s/lib_pecho_wchar$o

../obj_s/lib_slk_wset$o :	$(wide)/lib_slk_wset.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_slk_wset.c -o ../obj_s/lib_slk_wset$o

../obj_s/lib_unget_wch$o :	$(wide)/lib_unget_wch.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_unget_wch.c -o ../obj_s/lib_unget_wch$o

../obj_s/lib_vid_attr$o :	$(wide)/lib_vid_attr.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_vid_attr.c -o ../obj_s/lib_vid_attr$o

../obj_s/lib_vline_set$o :	$(wide)/lib_vline_set.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_vline_set.c -o ../obj_s/lib_vline_set$o

../obj_s/lib_wacs$o :	$(wide)/lib_wacs.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_wacs.c -o ../obj_s/lib_wacs$o

../obj_s/lib_wunctrl$o :	$(wide)/lib_wunctrl.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(wide)/lib_wunctrl.c -o ../obj_s/lib_wunctrl$o

../obj_s/expanded$o :	./expanded.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/expanded.c -o ../obj_s/expanded$o

../obj_s/legacy_coding$o :	$(base)/legacy_coding.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/legacy_coding.c -o ../obj_s/legacy_coding$o

../obj_s/lib_dft_fgbg$o :	$(base)/lib_dft_fgbg.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/lib_dft_fgbg.c -o ../obj_s/lib_dft_fgbg$o

../obj_s/lib_print$o :	$(tinfo)/lib_print.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_print.c -o ../obj_s/lib_print$o

../obj_s/resizeterm$o :	$(base)/resizeterm.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/resizeterm.c -o ../obj_s/resizeterm$o

../obj_s/use_screen$o :	$(tinfo)/use_screen.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/use_screen.c -o ../obj_s/use_screen$o

../obj_s/use_window$o :	$(base)/use_window.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/use_window.c -o ../obj_s/use_window$o

../obj_s/wresize$o :	$(base)/wresize.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/wresize.c -o ../obj_s/wresize$o

../obj_s/access$o :	$(tinfo)/access.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/access.c -o ../obj_s/access$o

../obj_s/add_tries$o :	$(tinfo)/add_tries.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/add_tries.c -o ../obj_s/add_tries$o

../obj_s/alloc_ttype$o :	$(tinfo)/alloc_ttype.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/alloc_ttype.c -o ../obj_s/alloc_ttype$o

../obj_s/codes$o :	./codes.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/codes.c -o ../obj_s/codes$o

../obj_s/comp_error$o :	$(tinfo)/comp_error.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/comp_error.c -o ../obj_s/comp_error$o

../obj_s/db_iterator$o :	$(tinfo)/db_iterator.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/db_iterator.c -o ../obj_s/db_iterator$o

../obj_s/doalloc$o :	$(tinfo)/doalloc.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/doalloc.c -o ../obj_s/doalloc$o

../obj_s/entries$o :	$(tinfo)/entries.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/entries.c -o ../obj_s/entries$o

../obj_s/fallback$o :	./fallback.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/fallback.c -o ../obj_s/fallback$o

../obj_s/free_ttype$o :	$(tinfo)/free_ttype.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/free_ttype.c -o ../obj_s/free_ttype$o

../obj_s/getenv_num$o :	$(tinfo)/getenv_num.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/getenv_num.c -o ../obj_s/getenv_num$o

../obj_s/home_terminfo$o :	$(tinfo)/home_terminfo.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/home_terminfo.c -o ../obj_s/home_terminfo$o

../obj_s/init_keytry$o :	$(tinfo)/init_keytry.c \
			../include/term.h \
			$(INCDIR)/tic.h \
			init_keytry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/init_keytry.c -o ../obj_s/init_keytry$o

../obj_s/lib_acs$o :	$(tinfo)/lib_acs.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_acs.c -o ../obj_s/lib_acs$o

../obj_s/lib_baudrate$o :	$(tinfo)/lib_baudrate.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_baudrate.c -o ../obj_s/lib_baudrate$o

../obj_s/lib_cur_term$o :	$(tinfo)/lib_cur_term.c \
			../include/term.h \
			$(INCDIR)/term_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_cur_term.c -o ../obj_s/lib_cur_term$o

../obj_s/lib_data$o :	$(tinfo)/lib_data.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_data.c -o ../obj_s/lib_data$o

../obj_s/lib_has_cap$o :	$(tinfo)/lib_has_cap.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_has_cap.c -o ../obj_s/lib_has_cap$o

../obj_s/lib_kernel$o :	$(tinfo)/lib_kernel.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_kernel.c -o ../obj_s/lib_kernel$o

../obj_s/lib_keyname$o :	./lib_keyname.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/lib_keyname.c -o ../obj_s/lib_keyname$o

../obj_s/lib_longname$o :	$(tinfo)/lib_longname.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_longname.c -o ../obj_s/lib_longname$o

../obj_s/lib_napms$o :	$(tinfo)/lib_napms.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_napms.c -o ../obj_s/lib_napms$o

../obj_s/lib_options$o :	$(tinfo)/lib_options.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_options.c -o ../obj_s/lib_options$o

../obj_s/lib_raw$o :	$(tinfo)/lib_raw.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_raw.c -o ../obj_s/lib_raw$o

../obj_s/lib_setup$o :	$(tinfo)/lib_setup.c \
			../include/term.h \
			$(INCDIR)/term_entry.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_setup.c -o ../obj_s/lib_setup$o

../obj_s/lib_termcap$o :	$(tinfo)/lib_termcap.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h \
			$(INCDIR)/capdefaults.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_termcap.c -o ../obj_s/lib_termcap$o

../obj_s/lib_termname$o :	$(tinfo)/lib_termname.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_termname.c -o ../obj_s/lib_termname$o

../obj_s/lib_tgoto$o :	$(tinfo)/lib_tgoto.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_tgoto.c -o ../obj_s/lib_tgoto$o

../obj_s/lib_ti$o :	$(tinfo)/lib_ti.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_ti.c -o ../obj_s/lib_ti$o

../obj_s/lib_tparm$o :	$(tinfo)/lib_tparm.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_tparm.c -o ../obj_s/lib_tparm$o

../obj_s/lib_tputs$o :	$(tinfo)/lib_tputs.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_tputs.c -o ../obj_s/lib_tputs$o

../obj_s/lib_trace$o :	$(trace)/lib_trace.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(trace)/lib_trace.c -o ../obj_s/lib_trace$o

../obj_s/lib_ttyflags$o :	$(tinfo)/lib_ttyflags.c \
			../include/term.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/lib_ttyflags.c -o ../obj_s/lib_ttyflags$o

../obj_s/lib_twait$o :	$(serial)/lib_twait.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(serial)/lib_twait.c -o ../obj_s/lib_twait$o

../obj_s/name_match$o :	$(tinfo)/name_match.c \
			../include/term.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/name_match.c -o ../obj_s/name_match$o

../obj_s/names$o :	./names.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/names.c -o ../obj_s/names$o

../obj_s/read_entry$o :	$(tinfo)/read_entry.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/read_entry.c -o ../obj_s/read_entry$o

../obj_s/read_termcap$o :	$(tinfo)/read_termcap.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/read_termcap.c -o ../obj_s/read_termcap$o

../obj_s/setbuf$o :	$(tinfo)/setbuf.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/setbuf.c -o ../obj_s/setbuf$o

../obj_s/strings$o :	$(tinfo)/strings.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/strings.c -o ../obj_s/strings$o

../obj_s/tries$o :	$(base)/tries.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/tries.c -o ../obj_s/tries$o

../obj_s/trim_sgr0$o :	$(tinfo)/trim_sgr0.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/trim_sgr0.c -o ../obj_s/trim_sgr0$o

../obj_s/unctrl$o :	./unctrl.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/unctrl.c -o ../obj_s/unctrl$o

../obj_s/visbuf$o :	$(trace)/visbuf.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(trace)/visbuf.c -o ../obj_s/visbuf$o

../obj_s/alloc_entry$o :	$(tinfo)/alloc_entry.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/alloc_entry.c -o ../obj_s/alloc_entry$o

../obj_s/captoinfo$o :	$(tinfo)/captoinfo.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/captoinfo.c -o ../obj_s/captoinfo$o

../obj_s/comp_captab$o :	./comp_captab.c \
			$(INCDIR)/tic.h \
			../include/term.h \
			../include/hashsize.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c ../ncurses/comp_captab.c -o ../obj_s/comp_captab$o

../obj_s/comp_expand$o :	$(tinfo)/comp_expand.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/comp_expand.c -o ../obj_s/comp_expand$o

../obj_s/comp_hash$o :	$(tinfo)/comp_hash.c \
			../include/term.h \
			$(INCDIR)/tic.h \
			../include/hashsize.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/comp_hash.c -o ../obj_s/comp_hash$o

../obj_s/comp_parse$o :	$(tinfo)/comp_parse.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/comp_parse.c -o ../obj_s/comp_parse$o

../obj_s/comp_scan$o :	$(tinfo)/comp_scan.c \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/comp_scan.c -o ../obj_s/comp_scan$o

../obj_s/parse_entry$o :	$(tinfo)/parse_entry.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h \
			../include/parametrized.h \
			$(INCDIR)/capdefaults.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/parse_entry.c -o ../obj_s/parse_entry$o

../obj_s/write_entry$o :	$(tinfo)/write_entry.c \
			../include/term.h \
			$(INCDIR)/term_entry.h \
			$(INCDIR)/tic.h
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/write_entry.c -o ../obj_s/write_entry$o

../obj_s/define_key$o :	$(base)/define_key.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/define_key.c -o ../obj_s/define_key$o

../obj_s/hashed_db$o :	$(tinfo)/hashed_db.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(tinfo)/hashed_db.c -o ../obj_s/hashed_db$o

../obj_s/key_defined$o :	$(base)/key_defined.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/key_defined.c -o ../obj_s/key_defined$o

../obj_s/keybound$o :	$(base)/keybound.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/keybound.c -o ../obj_s/keybound$o

../obj_s/keyok$o :	$(base)/keyok.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/keyok.c -o ../obj_s/keyok$o

../obj_s/version$o :	$(base)/version.c
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(base)/version.c -o ../obj_s/version$o

depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
