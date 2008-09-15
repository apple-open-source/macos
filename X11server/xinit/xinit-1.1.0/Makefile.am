# 
#  Copyright 2005  Red Hat, Inc.
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Red Hat not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Red Hat makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
# 
#  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

XINITDIR = $(libdir)/X11/xinit

bin_PROGRAMS = xinit
bin_SCRIPTS = startx

xinit_CFLAGS = $(XINIT_CFLAGS) -DXINITDIR=\"$(XINITDIR)\" -DBINDIR=\"$(bindir)\"
xinit_LDADD = $(XINIT_LIBS)

xinit_SOURCES =	\
        xinit.c

appmandir = $(APP_MAN_DIR)

appman_PRE = \
	startx.man \
        xinit.man

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

include $(top_srcdir)/cpprules.in

xinitrcdir = $(XINITDIR)

PROGCPPDEFS = \
	-DXRDB=@XRDB@ \
	-DXMODMAP=@XMODMAP@ \
	-DTWM=@TWM@ \
	-DXCLOCK=@XCLOCK@ \
	-DXTERM=@XTERM@ \
	-DXSERVER=@XSERVER@ \
	-DXAUTH=@XAUTH@ \
	-DXINIT=@XINIT@

CPP_FILES_FLAGS = \
	-DXINITDIR=$(XINITDIR) $(PROGCPPDEFS) -DLIBDIR=$(libdir) \
	-DSHELL_CMD=$(SHELL_CMD) $(STARTX_COOKIE_FLAGS) \
	-D__libexecdir__="$(libexecdir)" \
	-D__bindir__="$(bindir)"

if LAUNCHD
launchagents_PRE = org.x.startx.plist.cpp
launchagents_DATA = $(launchagents_PRE:plist.cpp=plist)

SUBDIRS = privileged_startx
endif

DIST_SUBDIRS = privileged_startx

xinitrc: xinitrc.cpp Makefile
startx: startx.cpp Makefile

xinitrc_DATA = xinitrc

CLEANFILES = xinitrc startx $(appman_DATA) $(launchagents_DATA)

EXTRA_DIST = xinitrc.cpp startx.cpp org.x.startx.plist.cpp $(appman_PRE)	\
		startx.cmd xinitrc.cmd xinit.def ChangeLog autogen.sh

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
