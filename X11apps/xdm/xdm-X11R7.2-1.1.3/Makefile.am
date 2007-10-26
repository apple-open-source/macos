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

SUBDIRS = config greeter

bin_PROGRAMS = xdm xdmshell

xdmlibdir = $(XDMLIBDIR)
xdmlib_PROGRAMS = chooser

#
# xdm
#

xdm_CFLAGS = $(XDM_CFLAGS) -D_BSD_SOURCE
xdm_LDADD = $(XDM_LIBS) $(XDM_LDFLAGS)

xdm_SOURCES =	\
        auth.c \
        daemon.c \
        server.c \
        dpylist.c \
        dm.c \
        error.c \
        file.c \
        netaddr.c \
        reset.c \
        resource.c \
        protodpy.c \
        policy.c \
        session.c \
        socket.c \
        streams.c \
        util.c \
        xdmcp.c \
        mitauth.c \
        genauth.c \
        access.c \
        choose.c \
        dm_auth.h \
        dm_error.h \
        dm.h \
        dm_socket.h \
        greet.h

if HAS_XDM_AUTH
xdm_CFLAGS +=  -DHASXDMAUTH
xdm_SOURCES += xdmauth.c
endif

if !HAS_ARC4_RANDOM
xdm_SOURCES += prngc.c
endif

if HAS_SECURE_RPC
xdm_CFLAGS += -DSECURE_RPC
xdm_SOURCES += rpcauth.c
endif

if HAS_KERBEROS_FIVE
xdm_SOURCES += krb5auth.c
endif

#
# greeter
#

if !DYNAMIC_GREETER
xdm_SOURCES += \
	greeter/greet.c		\
	greeter/Login.c		\
	greeter/Login.h		\
	greeter/LoginP.h	\
	greeter/verify.c
xdm_CFLAGS += $(XDMGREET_CFLAGS)
endif


#
#  xdm man page 
#

appmandir = $(APP_MAN_DIR)

appman_PRE = xdm.man.cpp
appman_DATA = $(appman_PRE:man.cpp=@APP_MAN_SUFFIX@)

CLEANFILES = $(appman_DATA)
EXTRA_DIST = $(appman_PRE) ChangeLog

include $(top_srcdir)/cpprules.in

XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

MANDEFS = \
	-D__xorgversion__="\"$(XORGRELSTRING)\" \"$(XORGMANNAME)\"" \
	-DCHOOSERPATH=$(xdmlibdir)/chooser -DXDMLOGDIR=$(XDMLOGDIR) \
	-DXDMDIR=$(XDMCONFIGDIR) -DBINDIR=$(bindir) -DXDMPIDDIR=$(XDMPIDDIR) \
	-D__appmansuffix__=$(APP_MAN_SUFFIX) \
	-D__filemansuffix__=$(FILE_MAN_SUFFIX) \
	-D__libmansuffix__=$(LIB_MAN_SUFFIX) \
	-D__miscmansuffix__=$(MISC_MAN_SUFFIX) \
	-D__drivermansuffix__=$(DRIVER_MAN_SUFFIX) \
	-D__adminmansuffix__=$(ADMIN_MAN_SUFFIX) 

CPP_FILES_FLAGS = $(MANDEFS) $(RANDOMMANDEFS) $(EXTRAMANDEFS)

SUFFIXES += .$(APP_MAN_SUFFIX) .man.cpp

.man.cpp.$(APP_MAN_SUFFIX):
	$(RAWCPP) $(RAWCPPFLAGS) $(CPP_FILES_FLAGS) < $< | $(CPP_SED_MAGIC) > $@

#
#  xdmshell
#
xdmshell_CFLAGS = $(XDMSHELL_CFLAGS) -DBINDIR="\"$(bindir)\""


xdmshell_SOURCES =	\
        xdmshell.c

#
#  chooser
#

chooser_CFLAGS = $(CHOOSER_CFLAGS)
chooser_LDADD = $(CHOOSER_LIBS)

chooser_SOURCES =	\
	chooser.c

# App default files  (*.ad)

appdefaultdir = @appdefaultdir@

APPDEFAULTFILES = Chooser

SUFFIXES += .ad

.ad:
	cp $< $@

appdefault_DATA = $(APPDEFAULTFILES)

CLEANFILES += $(APPDEFAULTFILES)

EXTRA_DIST += $(APPDEFAULTFILES:%=%.ad)

if LINT
ALL_LINT_FLAGS=$(LINT_FLAGS) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS)

lint:
	$(LINT) $(ALL_LINT_FLAGS) $(xdm_CFLAGS) $(xdm_SOURCES) $(xdm_LDADD)
	$(LINT) $(ALL_LINT_FLAGS) $(xdmshell_CFLAGS) $(xdmshell_SOURCES)
	$(LINT) $(ALL_LINT_FLAGS) $(chooser_CFLAGS) $(chooser_SOURCES) $(chooser_LDADD)
	(cd greeter && $(MAKE) $(AM_MAKEFLAGS) lint)
endif LINT

.PHONY: ChangeLog

ChangeLog:
	(GIT_DIR=$(top_srcdir)/.git git-log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

