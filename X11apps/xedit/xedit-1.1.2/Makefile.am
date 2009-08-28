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

bin_PROGRAMS = xedit
noinst_LIBRARIES = liblisp.a libre.a libmp.a
noinst_PROGRAMS = lisp/lsp lisp/re/tests

#
#    libmp.a
#

libmp_a_CFLAGS =
libmp_a_SOURCES = \
	lisp/mp/mp.c	\
	lisp/mp/mp.h	\
	lisp/mp/mpi.c	\
	lisp/mp/mpr.c

#
#    liblisp.a
#

liblisp_a_CFLAGS = -I$(top_srcdir)/lisp/re -I$(top_srcdir)/lisp/mp -DLISP -DLISPDIR=\"$(LISPDIR)\" $(PKGDEPS_CFLAGS)
liblisp_a_SOURCES = \
	lisp/bytecode.c \
	lisp/bytecode.h \
	lisp/core.c \
	lisp/core.h \
	lisp/debugger.c \
	lisp/debugger.h \
	lisp/format.c \
	lisp/format.h \
	lisp/hash.c \
	lisp/hash.h \
	lisp/helper.c \
	lisp/helper.h \
	lisp/internal.h \
	lisp/io.c \
	lisp/io.h \
	lisp/lisp.c \
	lisp/lisp.h \
	lisp/math.c \
	lisp/math.h \
	lisp/package.c \
	lisp/package.h \
	lisp/pathname.c \
	lisp/pathname.h \
	lisp/private.h \
	lisp/read.c \
	lisp/read.h \
	lisp/regex.c \
	lisp/regex.h \
	lisp/require.c \
	lisp/require.h \
	lisp/stream.c \
	lisp/stream.h \
	lisp/string.c \
	lisp/string.h \
	lisp/struct.c \
	lisp/struct.h \
	lisp/time.c \
	lisp/time.h \
	lisp/write.c \
	lisp/write.h \
	lisp/xedit.c \
	lisp/xedit.h

if NEED_UNSETENV
liblisp_a_SOURCES += lisp/env.c
endif

#
#     libre.a
#

libre_a_CFLAGS = 
libre_a_SOURCES = \
	lisp/re/re.c \
	lisp/re/rec.c \
	lisp/re/re.h \
	lisp/re/reo.c \
	lisp/re/rep.h

#
#     xedit
#   
xedit_DEPENDENCIES = liblisp.a libmp.a libre.a
xedit_CFLAGS = $(PKGDEPS_CFLAGS) -I$(top_srcdir)/lisp/re -D_BSD_SOURCE -DXEDIT
xedit_LDADD = -L. -lre -llisp -lmp $(PKGDEPS_LIBS) -lm

xedit_SOURCES =	\
        commands.c \
	hash.c \
        hook.c \
        ispell.c \
        lisp.c \
        options.c \
	tags.c \
        util.c \
	util.h \
        xedit.c \
	xedit.h

if NEED_REALPATH
xedit_SOURCES += realpath.c
endif

if NEED_STRCASECMP
xedit_SOURCES += strcasecmp.c
endif

if USE_XPRINT
xedit_CFLAGS += -DINCLUDE_XPRINT_SUPPORT

xedit_SOURCES += \
        print.c \
        printdialog.c \
        printdialog.h \
        printdialogprivates.h \
        print.h
endif

# lisp/lsp
lisp_lsp_DEPENDENCIES = liblisp.a libre.a libmp.a
lisp_lsp_CFLAGS = -I$(top_srcdir)/lisp/re -I$(top_srcdir)/lisp/mp -DLISP -DLISPDIR=\"@LISPDIR@\" -D_BSD_SOURCE
lisp_lsp_LDADD = -L. -llisp -lre -lmp -lm
lisp_lsp_SOURCES = \
	hash.c \
	lisp/lsp.c

if NEED_REALPATH
lisp_lsp_SOURCES += realpath.c
endif

if NEED_STRCASECMP
lisp_lsp_SOURCES += strcasecmp.c
endif

# re/tests
lisp_re_tests_DEPENDENCIES = libre.a
lisp_re_tests_CFLAGS = -I$(top_srcdir)/lisp/re -D_BSD_SOURCE
lisp_re_tests_LDADD = -L. -lre
lisp_re_tests_SOURCES = lisp/re/tests.c


# App default files  (*.ad)

APPDEFAULTFILES = \
        app-defaults/Xedit-color \
	app-defaults/Xedit

if USE_XPRINT
app-defaults/Xedit.ad: 
	test -d app-defaults || mkdir app-defaults
	cp $(top_srcdir)/app-defaults/Xedit-xprint.ad app-defaults/Xedit.ad
else
app-defaults/Xedit.ad:
	test -d app-defaults || mkdir app-defaults
	cp $(top_srcdir)/app-defaults/Xedit-noxprint.ad app-defaults/Xedit.ad
endif

SUFFIXES = .ad 

.ad:
	test -d app-defaults || mkdir app-defaults
	cp $< $@

appdefaultdir = @appdefaultdir@
appdefault_DATA = $(APPDEFAULTFILES)

CLEANFILES = $(APPDEFAULTFILES) app-defaults/Xedit.ad

lispdir = $(LISPDIR)
dist_lisp_DATA = ${srcdir}/lisp/modules/lisp.lsp \
	${srcdir}/lisp/modules/xedit.lsp \
	${srcdir}/lisp/modules/syntax.lsp \
	${srcdir}/lisp/modules/indent.lsp

progmodesdir = $(LISPDIR)/progmodes
dist_progmodes_DATA = \
	${srcdir}/lisp/modules/progmodes/auto.lsp \
	${srcdir}/lisp/modules/progmodes/c.lsp \
	${srcdir}/lisp/modules/progmodes/html.lsp \
	${srcdir}/lisp/modules/progmodes/imake.lsp \
	${srcdir}/lisp/modules/progmodes/lisp.lsp \
	${srcdir}/lisp/modules/progmodes/make.lsp \
	${srcdir}/lisp/modules/progmodes/man.lsp \
	${srcdir}/lisp/modules/progmodes/patch.lsp \
	${srcdir}/lisp/modules/progmodes/perl.lsp \
	${srcdir}/lisp/modules/progmodes/python.lsp \
	${srcdir}/lisp/modules/progmodes/rpm.lsp \
	${srcdir}/lisp/modules/progmodes/sgml.lsp \
	${srcdir}/lisp/modules/progmodes/sh.lsp \
	${srcdir}/lisp/modules/progmodes/xconf.lsp \
	${srcdir}/lisp/modules/progmodes/xlog.lsp \
	${srcdir}/lisp/modules/progmodes/xrdb.lsp

appman_PRE = xedit.man

EXTRA_DIST = \
	app-defaults/Xedit-color.ad \
	app-defaults/Xedit-xprint.ad \
	app-defaults/Xedit-noxprint.ad \
	app-defaults/Xedit-sample \
	lisp/README \
	lisp/TODO \
	lisp/compile.c \
	lisp/env.c \
	lisp/mathimp.c \
	lisp/modules/psql.c \
	lisp/modules/x11.c \
	lisp/modules/xaw.c \
	lisp/modules/xt.c \
	lisp/re/README \
	lisp/re/tests.txt \
	lisp/test/hello.lsp \
	lisp/test/list.lsp \
	lisp/test/math.lsp \
	lisp/test/psql-1.lsp \
	lisp/test/psql-2.lsp \
	lisp/test/psql-3.lsp \
	lisp/test/regex.lsp \
	lisp/test/stream.lsp \
	lisp/test/widgets.lsp

appmandir = $(APP_MAN_DIR)

appman_DATA = $(appman_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST += $(appman_PRE)
CLEANFILES += $(appman_DATA)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MAN_SUBSTS = \
	-e 's|__vendorversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xorgversion__|"$(XORGRELSTRING)" "$(XORGMANNAME)"|' \
	-e 's|__xservername__|Xorg|g' \
	-e 's|__xconfigfile__|xorg.conf|g' \
	-e 's|__projectroot__|$(prefix)|g' \
	-e 's|__apploaddir__|$(appdefaultdir)|' \
	-e 's|__appmansuffix__|$(APP_MAN_SUFFIX)|g' \
	-e 's|__libmansuffix__|$(LIB_MAN_SUFFIX)|g' \
	-e 's|__adminmansuffix__|$(ADMIN_MAN_SUFFIX)|g' \
	-e 's|__miscmansuffix__|$(MISC_MAN_SUFFIX)|g' \
	-e 's|__filemansuffix__|$(FILE_MAN_SUFFIX)|g'

SUFFIXES += .$(APP_MAN_SUFFIX) .man

.man.$(APP_MAN_SUFFIX):
	sed $(MAN_SUBSTS) < $< > $@

EXTRA_DIST += ChangeLog
MAINTAINERCLEANFILES = ChangeLog

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
