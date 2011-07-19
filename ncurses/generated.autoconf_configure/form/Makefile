# $Id: Makefile.in,v 1.47 2007/04/28 14:56:11 tom Exp $
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
# Makefile for form source code.
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
srcdir		= /tmp/ncurses-27.roots/ncurses-27/ncurses/form
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

LIBRARIES	=  ../lib/libform.${ABI_VERSION}.dylib

LINT		= 
LINT_OPTS	= 
LINT_LIBS	= -lform -lncurses 

AUTO_SRC	= \
		../include/form.h

################################################################################
all \
libs \
install ::	$(AUTO_SRC) $(LIBRARIES)

sources :	$(AUTO_SRC)

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) :
	sh $(srcdir)/../mkdirs.sh $@

# make copies to simplify include-paths while still keeping form's include
# file in this directory.
../include/form.h : $(srcdir)/form.h
	-rm -f $@
	cp $(srcdir)/form.h $@

FORM_PRIV_H = \
	$(srcdir)/form.priv.h \
	$(srcdir)/form.h \
	../include/mf_common.h \
	../include/curses.h \
	../include/eti.h

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

../include/mf_common.h \
../include/eti.h :
	cd ../menu && $(MAKE) $@

###############################################################################
# The remainder of this file is automatically generated during configuration
###############################################################################

# generated by mk-0th.awk
#   libname:    form
#   subsets:    ticlib+termlib+ext_tinfo+base+widechar+ext_funcs

.SUFFIXES: .c .cc .h .i .ii
.c.i :
	$(CPP) $(CPPFLAGS) $< >$@
.cc.ii :
	$(CPP) $(CPPFLAGS) $< >$@
.h.i :
	$(CPP) $(CPPFLAGS) $< >$@

C_SRC = \
	$(srcdir)/f_trace.c \
	$(srcdir)/fld_arg.c \
	$(srcdir)/fld_attr.c \
	$(srcdir)/fld_current.c \
	$(srcdir)/fld_def.c \
	$(srcdir)/fld_dup.c \
	$(srcdir)/fld_ftchoice.c \
	$(srcdir)/fld_ftlink.c \
	$(srcdir)/fld_info.c \
	$(srcdir)/fld_just.c \
	$(srcdir)/fld_link.c \
	$(srcdir)/fld_max.c \
	$(srcdir)/fld_move.c \
	$(srcdir)/fld_newftyp.c \
	$(srcdir)/fld_opts.c \
	$(srcdir)/fld_pad.c \
	$(srcdir)/fld_page.c \
	$(srcdir)/fld_stat.c \
	$(srcdir)/fld_type.c \
	$(srcdir)/fld_user.c \
	$(srcdir)/frm_cursor.c \
	$(srcdir)/frm_data.c \
	$(srcdir)/frm_def.c \
	$(srcdir)/frm_driver.c \
	$(srcdir)/frm_hook.c \
	$(srcdir)/frm_opts.c \
	$(srcdir)/frm_page.c \
	$(srcdir)/frm_post.c \
	$(srcdir)/frm_req_name.c \
	$(srcdir)/frm_scale.c \
	$(srcdir)/frm_sub.c \
	$(srcdir)/frm_user.c \
	$(srcdir)/frm_win.c \
	$(srcdir)/fty_alnum.c \
	$(srcdir)/fty_alpha.c \
	$(srcdir)/fty_enum.c \
	$(srcdir)/fty_int.c \
	$(srcdir)/fty_ipv4.c \
	$(srcdir)/fty_num.c \
	$(srcdir)/fty_regex.c

# Producing llib-lform is time-consuming, so there's no direct-dependency for
# it in the lintlib rule.  We'll only remove in the cleanest setup.
clean ::
	rm -f llib-lform.*

realclean ::
	rm -f llib-lform

llib-lform : $(C_SRC)
	cproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) $(C_SRC) >$@

lintlib :
	sh $(srcdir)/../misc/makellib form $(CPPFLAGS)
lint :
	$(LINT) $(LINT_OPTS) $(CPPFLAGS) $(C_SRC) $(LINT_LIBS)

# generated by mk-1st.awk (subset=ticlib+termlib+ext_tinfo+base+widechar+ext_funcs)
#  name:          form
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
#  depend:        ../include/ncurses_cfg.h ${srcdir}/form.priv.h
#  host:          i386-apple-darwin10.0

SHARED_OBJS = \
	../obj_s/fld_arg$o \
	../obj_s/fld_attr$o \
	../obj_s/fld_current$o \
	../obj_s/fld_def$o \
	../obj_s/fld_dup$o \
	../obj_s/fld_ftchoice$o \
	../obj_s/fld_ftlink$o \
	../obj_s/fld_info$o \
	../obj_s/fld_just$o \
	../obj_s/fld_link$o \
	../obj_s/fld_max$o \
	../obj_s/fld_move$o \
	../obj_s/fld_newftyp$o \
	../obj_s/fld_opts$o \
	../obj_s/fld_pad$o \
	../obj_s/fld_page$o \
	../obj_s/fld_stat$o \
	../obj_s/fld_type$o \
	../obj_s/fld_user$o \
	../obj_s/frm_cursor$o \
	../obj_s/frm_data$o \
	../obj_s/frm_def$o \
	../obj_s/frm_driver$o \
	../obj_s/frm_hook$o \
	../obj_s/frm_opts$o \
	../obj_s/frm_page$o \
	../obj_s/frm_post$o \
	../obj_s/frm_req_name$o \
	../obj_s/frm_scale$o \
	../obj_s/frm_sub$o \
	../obj_s/frm_user$o \
	../obj_s/frm_win$o \
	../obj_s/fty_alnum$o \
	../obj_s/fty_alpha$o \
	../obj_s/fty_enum$o \
	../obj_s/fty_int$o \
	../obj_s/fty_ipv4$o \
	../obj_s/fty_num$o \
	../obj_s/fty_regex$o

$(SHARED_OBJS) : ../include/ncurses_cfg.h ${srcdir}/form.priv.h

../lib/libform.$(ABI_VERSION).dylib : \
		../lib \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd ../lib && (rm -f libform.dylib; $(LN_S) libform.$(ABI_VERSION).dylib libform.dylib; )

install \
install.libs \
install.form :: $(DESTDIR)$(libdir)/libform.$(ABI_VERSION).dylib

$(DESTDIR)$(libdir)/libform.$(ABI_VERSION).dylib : \
		$(DESTDIR)$(libdir) \
		$(SHARED_OBJS)
	@echo linking $@
	$(MK_SHARED_LIB) $(SHARED_OBJS) $(SHLIB_LIST) $(LDFLAGS)
	cd $(DESTDIR)$(libdir) && (rm -f libform.dylib; $(LN_S) libform.$(ABI_VERSION).dylib libform.dylib; )

uninstall \
uninstall.libs \
uninstall.form ::
	@echo uninstalling $(DESTDIR)$(libdir)/libform.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libform.$(ABI_VERSION).dylib
	-rm -f $(DESTDIR)$(libdir)/libform.dylib

clean ::
	-rm -f ../lib/libform.$(ABI_VERSION).dylib
	-rm -f ../lib/libform.dylib

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

../obj_s/fld_arg$o :	$(srcdir)/fld_arg.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_arg.c -o ../obj_s/fld_arg$o

../obj_s/fld_attr$o :	$(srcdir)/fld_attr.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_attr.c -o ../obj_s/fld_attr$o

../obj_s/fld_current$o :	$(srcdir)/fld_current.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_current.c -o ../obj_s/fld_current$o

../obj_s/fld_def$o :	$(srcdir)/fld_def.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_def.c -o ../obj_s/fld_def$o

../obj_s/fld_dup$o :	$(srcdir)/fld_dup.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_dup.c -o ../obj_s/fld_dup$o

../obj_s/fld_ftchoice$o :	$(srcdir)/fld_ftchoice.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_ftchoice.c -o ../obj_s/fld_ftchoice$o

../obj_s/fld_ftlink$o :	$(srcdir)/fld_ftlink.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_ftlink.c -o ../obj_s/fld_ftlink$o

../obj_s/fld_info$o :	$(srcdir)/fld_info.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_info.c -o ../obj_s/fld_info$o

../obj_s/fld_just$o :	$(srcdir)/fld_just.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_just.c -o ../obj_s/fld_just$o

../obj_s/fld_link$o :	$(srcdir)/fld_link.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_link.c -o ../obj_s/fld_link$o

../obj_s/fld_max$o :	$(srcdir)/fld_max.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_max.c -o ../obj_s/fld_max$o

../obj_s/fld_move$o :	$(srcdir)/fld_move.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_move.c -o ../obj_s/fld_move$o

../obj_s/fld_newftyp$o :	$(srcdir)/fld_newftyp.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_newftyp.c -o ../obj_s/fld_newftyp$o

../obj_s/fld_opts$o :	$(srcdir)/fld_opts.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_opts.c -o ../obj_s/fld_opts$o

../obj_s/fld_pad$o :	$(srcdir)/fld_pad.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_pad.c -o ../obj_s/fld_pad$o

../obj_s/fld_page$o :	$(srcdir)/fld_page.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_page.c -o ../obj_s/fld_page$o

../obj_s/fld_stat$o :	$(srcdir)/fld_stat.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_stat.c -o ../obj_s/fld_stat$o

../obj_s/fld_type$o :	$(srcdir)/fld_type.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_type.c -o ../obj_s/fld_type$o

../obj_s/fld_user$o :	$(srcdir)/fld_user.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fld_user.c -o ../obj_s/fld_user$o

../obj_s/frm_cursor$o :	$(srcdir)/frm_cursor.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_cursor.c -o ../obj_s/frm_cursor$o

../obj_s/frm_data$o :	$(srcdir)/frm_data.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_data.c -o ../obj_s/frm_data$o

../obj_s/frm_def$o :	$(srcdir)/frm_def.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_def.c -o ../obj_s/frm_def$o

../obj_s/frm_driver$o :	$(srcdir)/frm_driver.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_driver.c -o ../obj_s/frm_driver$o

../obj_s/frm_hook$o :	$(srcdir)/frm_hook.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_hook.c -o ../obj_s/frm_hook$o

../obj_s/frm_opts$o :	$(srcdir)/frm_opts.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_opts.c -o ../obj_s/frm_opts$o

../obj_s/frm_page$o :	$(srcdir)/frm_page.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_page.c -o ../obj_s/frm_page$o

../obj_s/frm_post$o :	$(srcdir)/frm_post.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_post.c -o ../obj_s/frm_post$o

../obj_s/frm_req_name$o :	$(srcdir)/frm_req_name.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_req_name.c -o ../obj_s/frm_req_name$o

../obj_s/frm_scale$o :	$(srcdir)/frm_scale.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_scale.c -o ../obj_s/frm_scale$o

../obj_s/frm_sub$o :	$(srcdir)/frm_sub.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_sub.c -o ../obj_s/frm_sub$o

../obj_s/frm_user$o :	$(srcdir)/frm_user.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_user.c -o ../obj_s/frm_user$o

../obj_s/frm_win$o :	$(srcdir)/frm_win.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/frm_win.c -o ../obj_s/frm_win$o

../obj_s/fty_alnum$o :	$(srcdir)/fty_alnum.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_alnum.c -o ../obj_s/fty_alnum$o

../obj_s/fty_alpha$o :	$(srcdir)/fty_alpha.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_alpha.c -o ../obj_s/fty_alpha$o

../obj_s/fty_enum$o :	$(srcdir)/fty_enum.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_enum.c -o ../obj_s/fty_enum$o

../obj_s/fty_int$o :	$(srcdir)/fty_int.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_int.c -o ../obj_s/fty_int$o

../obj_s/fty_ipv4$o :	$(srcdir)/fty_ipv4.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_ipv4.c -o ../obj_s/fty_ipv4$o

../obj_s/fty_num$o :	$(srcdir)/fty_num.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_num.c -o ../obj_s/fty_num$o

../obj_s/fty_regex$o :	$(srcdir)/fty_regex.c \
			$(FORM_PRIV_H)
	$(LIBTOOL_COMPILE) $(CC) $(CFLAGS_SHARED) -c $(srcdir)/fty_regex.c -o ../obj_s/fty_regex$o


# generated by mk-hdr.awk
#  subset:     ticlib+termlib+ext_tinfo+base+widechar+ext_funcs
#  compat:     yes

${DESTDIR}${includedir} :
	sh ${srcdir}/../mkdirs.sh $@

install \
install.libs \
install.includes :: ${AUTO_SRC} ${DESTDIR}${includedir} \
		$(srcdir)/form.h
	@ (cd ${DESTDIR}${includedir} && rm -f form.h) ; ../headers.sh ${INSTALL_DATA} ${DESTDIR}${includedir} ${srcdir} $(srcdir)/form.h

uninstall \
uninstall.libs \
uninstall.includes ::
	-@ (cd ${DESTDIR}${includedir} && rm -f form.h)
depend : ${AUTO_SRC}
	makedepend -- ${CPPFLAGS} -- ${C_SRC}

# DO NOT DELETE THIS LINE -- make depend depends on it.
