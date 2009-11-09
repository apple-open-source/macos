# Makefile for fetchmail

SUBDIRS=		. po
AUTOMAKE_OPTIONS=	1.8 foreign no-dist-gzip dist-bzip2

AM_CPPFLAGS=		-I$(srcdir)/libesmtp
ACLOCAL_AMFLAGS=	-I m4 -I m4-local
AM_YFLAGS=		-d
BUILT_SOURCES=		rcfile_y.h

bin_PROGRAMS=		fetchmail

dist_noinst_SCRIPTS=	specgen.sh
dist_man1_MANS=		fetchmail.man

pys=			fetchmailconf.py
pym=			fetchmailconf.man

if HAVE_PYTHON
nodist_bin_SCRIPTS=	fetchmailconf
python_PYTHON=		$(pys)
dist_man1_MANS+=	$(pym)
EXTRA_DIST=
else
noinst_PYTHON=		$(pys)
EXTRA_DIST=		$(pym)
endif

CLEANFILES=		$(nodist_bin_SCRIPTS)

# for gettext (used by fetchmail.c, NOT by GNU gettext)
localedir=		$(datadir)/locale
DEFS=			@DEFS@ -DLOCALEDIR=\"$(localedir)\"

noinst_LIBRARIES=	libfm.a
libfm_a_SOURCES=	xmalloc.c base64.c rfc822.c report.c rfc2047e.c \
			servport.c ntlm.h smbbyteorder.h smbdes.h smbmd4.h \
			smbencrypt.h smbdes.c smbencrypt.c smbmd4.c smbutil.c \
			libesmtp/gethostbyname.h libesmtp/gethostbyname.c \
			smbtypes.h fm_getaddrinfo.c tls.c rfc822valid.c \
			xmalloc.h sdump.h sdump.c
libfm_a_LIBADD=		$(EXTRAOBJ)
libfm_a_DEPENDENCIES=	$(EXTRAOBJ)
LDADD	=		libfm.a @LIBINTL@ $(LIBOBJS)
DEPENDENCIES=		libfm.a $(LIBOBJS)

check_PROGRAMS=

TESTS=			t.smoke
TESTS_ENVIRONMENT=	srcdir="@srcdir@" LC_ALL=C TZ=UTC

if NEED_TRIO
noinst_LIBRARIES+=	libtrio.a
libtrio_a_SOURCES=	trio/triostr.c trio/trio.c trio/trionan.c \
			trio/trio.h trio/triop.h trio/triodef.h \
			trio/trionan.h trio/triostr.h
check_PROGRAMS+=	regression
regression_SOURCES=	trio/regression.c
LDADD +=		libtrio.a -lm
TESTS+=			regression
endif

fetchmail_SOURCES=	fetchmail.h getopt.h \
		i18n.h kerberos.h md5.h mx.h netrc.h smtp.h \
		socket.h tunable.h \
		socket.c getpass.c \
		fetchmail.c env.c idle.c options.c daemon.c \
		driver.c transact.c sink.c smtp.c \
		uid.c mxget.c md5ify.c cram.c gssapi.c \
		opie.c interface.c netrc.c \
		unmime.c conf.c checkalias.c \
		lock.h lock.c \
		rcfile_l.l rcfile_y.y ucs/norm_charmap.c
if POP2_ENABLE
fetchmail_SOURCES += pop2.c
endif
if POP3_ENABLE
fetchmail_SOURCES += pop3.c
endif
if IMAP_ENABLE
fetchmail_SOURCES += imap.c
endif
if ETRN_ENABLE
fetchmail_SOURCES += etrn.c
endif
if ODMR_ENABLE
fetchmail_SOURCES += odmr.c
endif
if KERBEROS_V4_ENABLE
fetchmail_SOURCES += kerberos.c
endif
if RPA_ENABLE
fetchmail_SOURCES += rpa.c
endif
if NEED_GETNAMEINFO
fetchmail_SOURCES += KAME/getnameinfo.c
endif
if NEED_GETADDRINFO
fetchmail_SOURCES += libesmtp/getaddrinfo.h libesmtp/getaddrinfo.c
endif

check_PROGRAMS +=	rfc822 unmime netrc rfc2047e mxget rfc822valid

rfc2047e_CFLAGS=	-DTEST

rfc822valid_CFLAGS=	-DTEST

rfc822_CFLAGS=		-DMAIN

unmime_SOURCES=	unmime.c
unmime_CFLAGS=	-DSTANDALONE -DHAVE_CONFIG_H -I$(builddir)

netrc_SOURCES=	netrc.c xmalloc.c report.c
netrc_CFLAGS=	-DSTANDALONE -DHAVE_CONFIG_H -I$(builddir)

mxget_SOURCES=	mxget.c
mxget_CFLAGS=	-DSTANDALONE -DHAVE_CONFIG_H -I$(builddir)

# For some losing Unix makes.
SHELL = /bin/sh
@SET_MAKE@

fetchmail.spec: Makefile.in specgen.sh
	$(srcdir)/specgen.sh $(VERSION) >fetchmail.spec

DISTDOCS=	FAQ FEATURES NOTES OLDNEWS fetchmail-man.html \
		design-notes.html esrs-design-notes.html todo.html \
		fetchmail-features.html README.SSL README.NTLM \
		README.packaging README.SSL-SERVER \
		fetchmail-FAQ.book fetchmail-FAQ.pdf fetchmail-FAQ.html \
		fetchmail-SA-2009-01.txt \
		fetchmail-SA-2008-01.txt \
		fetchmail-SA-2007-02.txt \
		fetchmail-SA-2007-01.txt \
		fetchmail-SA-2006-03.txt \
		fetchmail-SA-2006-02.txt \
		fetchmail-SA-2006-01.txt \
		fetchmail-SA-2005-01.txt \
		fetchmail-SA-2005-02.txt \
		fetchmail-SA-2005-03.txt

# extra directories to ship
distdirs = rh-config contrib beos

EXTRA_DIST+=	$(DISTDOCS) fetchmail.spec $(distdirs) ucs/README.svn \
		trio/CHANGES trio/README \
		strlcpy.3 bighand.png \
		m4/codeset.m4 \
		m4/gettext.m4 \
		m4/glibc21.m4 \
		m4/iconv.m4 \
		m4/intdiv0.m4 \
		m4/intmax.m4 \
		m4/inttypes-pri.m4 \
		m4/inttypes.m4 \
		m4/inttypes_h.m4 \
		m4/isc-posix.m4 \
		m4/lcmessage.m4 \
		m4/lib-ld.m4 \
		m4/lib-link.m4 \
		m4/lib-prefix.m4 \
		m4/longdouble.m4 \
		m4/longlong.m4 \
		m4/nls.m4 \
		m4/po.m4 \
		m4/printf-posix.m4 \
		m4/progtest.m4 \
		m4/signed.m4 \
		m4/size_max.m4 \
		m4/stdint_h.m4 \
		m4/uintmax_t.m4 \
		m4/ulonglong.m4 \
		m4/wchar_t.m4 \
		m4/wint_t.m4 \
		m4/xsize.m4 \
		m4-local/ac-archive-license.txt \
		m4-local/ac_ma_search_package.m4 \
		t.smoke t.rc \
		dist-tools/html2txt \
		dist-tools/manServer.pl \
		Doxyfile

fetchmailconf:
	( echo '#! /bin/sh' && echo 'exec @PYTHON@ @pythondir@/fetchmailconf.py "$$@"' ) >$@ && chmod +x $@ || { rm -f $@ ; exit 1; }

FAQ: fetchmail-FAQ.html $(srcdir)/dist-tools/html2txt
	AWK=$(AWK) $(SHELL) $(srcdir)/dist-tools/html2txt $(srcdir)/fetchmail-FAQ.html >$@ || { rm -f $@ ; exit 1 ; }

fetchmail-FAQ.pdf: fetchmail-FAQ.html fetchmail-FAQ.book bighand.png
	$(AWK) '/^[ \t]*<h1/ {sec++; } {if (sec < 2 || sec > 3) print $0;}' <$(srcdir)/fetchmail-FAQ.html >fetchmail-FAQ-print.html
	htmldoc --logoimage $(srcdir)/bighand.png --batch $(srcdir)/fetchmail-FAQ.book || { rm -f $@ ; exit 1 ; }
	rm -f fetchmail-FAQ-print.html

FEATURES: fetchmail-features.html $(srcdir)/dist-tools/html2txt
	AWK=$(AWK) $(SHELL) $(srcdir)/dist-tools/html2txt $(srcdir)/fetchmail-features.html >$@ || { rm -f $@ ; exit 1 ; }

NOTES: design-notes.html esrs-design-notes.html $(srcdir)/dist-tools/html2txt
	   echo "This file contains two articles reformatted from HTML." > $@ \
	&& echo "------------------------------------------------------" >> $@ \
	&& echo "" >> $@ \
	&& AWK=$(AWK) $(SHELL) $(srcdir)/dist-tools/html2txt $(srcdir)/design-notes.html >>$@ \
	&& AWK=$(AWK) $(SHELL) $(srcdir)/dist-tools/html2txt $(srcdir)/esrs-design-notes.html >>$@ \
	|| { rm -f $@ ; exit 1 ; }

TODO: todo.html $(srcdir)/dist-tools/html2txt
	AWK=$(AWK) $(SHELL) $(srcdir)/dist-tools/html2txt $(srcdir)/todo.html >$@ || { rm -f $@ ; exit 1 ; }

fetchmail-man.html: fetchmail.man $(srcdir)/dist-tools/manServer.pl
	env - "PATH=$$PATH" $(srcdir)/dist-tools/manServer.pl $(srcdir)/fetchmail.man >$@ \
	|| { rm -f $@ ; exit 1 ; }

dist-hook:
	cd $(distdir) && find $(distdirs) po -name .svn -type d -prune -exec rm -rf '{}' ';'
	cd $(distdir) && find $(distdirs) po -name '*~' -exec rm -f '{}' ';'

.PHONY: lsm
lsm: genlsm.sh dist
	$(SHELL) genlsm.sh >$(PACKAGE)-$(VERSION).lsm

# this target expects a .rsyncs file with lines of this format:
# host:directory/
# it will call rsync from its source directory to the destination for
# each of them, running them in parallel
rsync:  distdir $(srcdir)/.rsyncs
	@( cat $(srcdir)/.rsyncs | sed -e 's}^}rsync -aH --delete-after $(PACKAGE)-$(VERSION)/ }; s/\($$\)/ \&/;' ; echo "wait" ) | $(SHELL) -x

# The following sets edit modes for GNU EMACS.
# Local Variables:
# compile-command:"configure"
# End:
