# $Id: Makefile.in,v 1.43 2007/03/31 15:54:17 tom Exp $
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
# Makefile for ncurses miscellany directory
#
# This makes/installs the terminfo database
#
# The variable 'srcdir' refers to the source-distribution, and can be set with
# the configure script by "--srcdir=DIR".
#
# The rules are organized to produce the libraries for the configured models,
# and the programs with the configured default model.

# turn off _all_ suffix rules; we'll generate our own
.SUFFIXES :

SHELL		= /bin/sh
THIS		= Makefile

CF_MFLAGS 	= 


DESTDIR		= 
top_srcdir	= /tmp/ncurses-27.roots/ncurses-27/ncurses
srcdir		= /tmp/ncurses-27.roots/ncurses-27/ncurses/misc
prefix		= /usr
exec_prefix	= ${prefix}
bindir		= ${exec_prefix}/bin
libdir		= ${exec_prefix}/lib
datadir		= ${prefix}/share

CC		= gcc
HOSTCC		= ${CC}

tabsetdir	= $(datadir)/tabset
ticdir		= /usr/share/terminfo
ticfile		= $(ticdir).db
source		= ${top_srcdir}/misc/terminfo.src

INSTALL		= /usr/bin/install -c
INSTALL_PROG	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

################################################################################
all :	terminfo.tmp

depend :

sources :

install : install.data install.libs

install.data :	terminfo.tmp \
		$(DESTDIR)$(libdir) \
		$(DESTDIR)$(datadir) \
		$(DESTDIR)$(tabsetdir)
	DESTDIR=${DESTDIR} \
	prefix=${prefix} \
	exec_prefix=${exec_prefix} \
	bindir=${bindir} \
	top_srcdir=${top_srcdir} \
	srcdir=${srcdir} \
	datadir=${datadir} \
	ticdir=${ticdir} \
	source=terminfo.tmp \
	THIS_CC="$(CC)" \
	THAT_CC="$(HOSTCC)" \
	$(SHELL) ./run_tic.sh
	@cd $(srcdir)/tabset && \
		$(SHELL) -c 'for i in * ; do \
			if test -f $$i ; then \
			echo installing $$i; \
			$(INSTALL_DATA) $$i $(DESTDIR)$(tabsetdir)/$$i; \
			fi; done'

NCURSES_CONFIG	= ncurses5.4-config

install.libs : $(DESTDIR)$(bindir) ncurses-config
	$(INSTALL_PROG) ncurses-config $(DESTDIR)$(bindir)/$(NCURSES_CONFIG)

terminfo.tmp : run_tic.sed $(source)
	echo '** adjusting tabset paths'
	sed -f run_tic.sed $(source) >terminfo.tmp

run_tic.sed :
	WHICH_XTERM=xterm-new \
	ticdir=${ticdir} \
	$(SHELL) $(srcdir)/gen_edit.sh >$@

$(DESTDIR)$(bindir) \
$(DESTDIR)$(libdir) \
$(DESTDIR)$(datadir) \
$(DESTDIR)$(tabsetdir) :
	$(SHELL) $(top_srcdir)/mkdirs.sh $@

uninstall : uninstall.data uninstall.libs

uninstall.data :
	-test -d $(DESTDIR)$(tabsetdir) && rm -rf $(DESTDIR)$(tabsetdir)
	-test -d $(DESTDIR)$(ticdir) && rm -rf $(DESTDIR)$(ticdir)
	-test -f $(DESTDIR)$(ticfile) && rm -f $(DESTDIR)$(ticfile)

uninstall.libs :
	-rm -f $(DESTDIR)$(bindir)/$(NCURSES_CONFIG)

tags :

#TAGS :

mostlyclean :
	-rm -f terminfo.tmp
	-rm -f run_tic.sed
	-rm -f core tags TAGS *~ *.bak *.ln *.atac trace

clean :: mostlyclean

distclean : clean
	-rm -f Makefile run_tic.sh ncurses-config

realclean : distclean

###############################################################################
# The remainder of this file is automatically generated during configuration
###############################################################################
