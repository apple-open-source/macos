# This is the auto-Makefile for Taylor UUCP
# This is processed by automake to create Makefile.in.

SUBDIRS = lib uuconf unix

sbin_PROGRAMS = uucico uuxqt uuchk uuconv
sbin_SCRIPTS = uusched
bin_PROGRAMS = uux uucp uustat uuname uulog uupick cu
bin_SCRIPTS = uuto
noinst_PROGRAMS = tstuu
info_TEXINFOS = uucp.texi
man_MANS = uux.1 uucp.1 uustat.1 cu.1 uucico.8 uuxqt.8

# If you do not have the mkdir system call, this will install uudir as
# an suid root program.  This is necessary because invoking /bin/mkdir
# from an suid program will leave the directories owned by the wrong
# user.
if HAVE_MKDIR
uudirdir =
uudir_PROGRAMS =
else
uudirdir = $(sbindir)/util
uudir_PROGRAMS = uudir
endif

AM_CFLAGS = $(WARN_CFLAGS) $(UUDIRFLAGS)

LDADD = unix/libunix.a uuconf/libuuconf.a lib/libuucp.a

UUHEADERS = uucp.h uudefs.h uuconf.h policy.h system.h sysdep.h getopt.h

uucico_SOURCES = uucico.c trans.h trans.c send.c rec.c xcmd.c prot.h prot.c \
	protg.c protf.c prott.c prote.c proti.c protj.c proty.c protz.c \
	time.c log.c chat.c conn.h conn.c util.c copy.c $(UUHEADERS)
uuxqt_SOURCES = uuxqt.c util.c log.c copy.c $(UUHEADERS)
uux_SOURCES = uux.c util.c log.c copy.c $(UUHEADERS)
uucp_SOURCES = uucp.c util.c log.c copy.c $(UUHEADERS)
uustat_SOURCES = uustat.c util.c log.c copy.c $(UUHEADERS)
uuname_SOURCES = uuname.c log.c $(UUHEADERS)
uulog_SOURCES = uulog.c log.c $(UUHEADERS)
uupick_SOURCES = uupick.c log.c copy.c $(UUHEADERS)
cu_SOURCES = cu.h cu.c prot.c log.c chat.c conn.c copy.c $(UUHEADERS)
uuchk_SOURCES = uuchk.c $(UUHEADERS)
uuconv_SOURCES = uuconv.c $(UUHEADERS)
tstuu_SOURCES = tstuu.c
uudir_SOURCES = uudir.c

uuconv_CFLAGS = -I$(srcdir)/uuconf $(AM_CFLAGS)

uusched: uusched.in Makefile
	-rm -f $@ uusched.t
	if [ "x$(POUNDBANG)" = "xno" ]; then \
	  sed -e 's,#!/bin/sh,:,' -e 's,@SBINDIR@,$(sbindir),' < $(srcdir)/uusched.in > uusched.t; \
	else \
	  sed -e 's,@SBINDIR@,$(sbindir),' < $(srcdir)/uusched.in > uusched.t; \
	fi
	chmod 0555 uusched.t
	mv -f uusched.t $@

uuto: uuto.in Makefile
	-rm -f $@ uuto.t
	if [ "x$(POUNDBANG)" = "xno" ]; then \
	  sed -e 's,#!/bin/sh,:,' -e 's,@BINDIR@,$(bindir),' -e 's,@VERS@,$(VERSION),' < $(srcdir)/uuto.in > uuto.t; \
	else \
	  sed -e 's,@BINDIR@,$(bindir),' -e 's,@VERS@,$(VERSION),' < $(srcdir)/uuto.in > uuto.t; \
	fi
	chmod 0555 uuto.t
	mv -f uuto.t $@

CLEANFILES = uusched uuto

EXTRA_DIST = uusched.in uuto.in \
	cu.1 uucp.1 uustat.1 uux.1 uucico.8 uuxqt.8 \
	contrib sample

dist-hook:
	-rm -rf $(distdir)/contrib/CVS $(distdir)/sample/CVS

install-exec-hook:
	for f in uucico uuxqt; do \
	  chown $(OWNER) $(DESTDIR)$(sbindir)/$${f}; \
	  chmod 4555 $(DESTDIR)$(sbindir)/$${f}; \
	done
	for f in uux uucp uustat uuname cu; do \
	  chown $(OWNER) $(DESTDIR)$(bindir)/$${f}; \
	  chmod 4555 $(DESTDIR)$(bindir)/$${f}; \
	done
if HAVE_MKDIR
else
	chown $(OWNER) $(DESTDIR)$(uudirdir)
	chmod 100 $(DESTDIR)$(uudirdir)
	chown root $(DESTDIR)$(uudirdir)/uudir
	chmod 4555 $(DESTDIR)$(uudirdir)/uudir
endif
