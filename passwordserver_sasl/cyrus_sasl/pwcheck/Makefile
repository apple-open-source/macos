# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.4 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# Makefile.am for the pwcheck daemon
# Larry Greenfield
#
#         Copyright 1999 by Carnegie Mellon University
#
#                       All Rights Reserved
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in
# supporting documentation, and that the name of CMU not be
# used in advertising or publicity pertaining to distribution of the
# software without specific, written prior permission.
#
# CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
# ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
# CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
# ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
# ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.
#


SHELL = /bin/sh

srcdir = .
top_srcdir = ..
prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/cyrus-sasl
pkglibdir = $(libdir)/cyrus-sasl
pkgincludedir = $(includedir)/cyrus-sasl

top_builddir = ..

ACLOCAL = /SASL/cyrus-sasl-2.1.1/cyrus-sasl-2.1.1/config/missing aclocal -I $(top_srcdir)/cmulocal -I $(top_srcdir)/config
AUTOCONF = autoconf
AUTOMAKE = /SASL/cyrus-sasl-2.1.1/cyrus-sasl-2.1.1/config/missing automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = powerpc-apple-darwin6.0
host_triplet = powerpc-apple-darwin6.0
AS = @AS@
AWK = awk
CC = cc
CPP = cc -E -traditional-cpp
DIRS = 
DLLTOOL = @DLLTOOL@
DMALLOC_LIBS = 
DYNSASLFLAGS = @DYNSASLFLAGS@
GETADDRINFOOBJS = 
GETNAMEINFOOBJS = 
GETSUBOPT = 
GSSAPIBASE_LIBS = 
GSSAPI_LIBS = 
JAVAC = true
JAVADOC = true
JAVAH = true
JAVAROOT = 
JAVA_INCLUDES = 
LIBOBJS = 
LIBTOOL = $(SHELL) $(top_builddir)/libtool
LIB_CRYPT = 
LIB_DES = -lcrypto
LIB_DYN_SASL = @LIB_DYN_SASL@
LIB_PAM = -lpam
LIB_SASL = @LIB_SASL@
LIB_SOCKET = 
LN_S = ln -s
LTGETADDRINFOOBJS = 
LTGETNAMEINFOOBJS = 
LTLIBOBJS = 
LTSNPRINTFOBJS = 
MAKEINFO = /SASL/cyrus-sasl-2.1.1/cyrus-sasl-2.1.1/config/missing makeinfo
NM = /usr/bin/nm -p
OBJDUMP = @OBJDUMP@
OTP_LIBS = -lcrypto ../sasldb/libsasldb.la $(SASL_DB_LIB)
PACKAGE = cyrus-sasl
PLAIN_LIBS = 
PURECOV = 
PURIFY = 
PWCHECKMETH = 
RANLIB = ranlib
SASLFLAGS = @SASLFLAGS@
SASL_DB_BACKEND = db_ndbm.lo
SASL_DB_BACKEND_STATIC = ../sasldb/db_ndbm.o ../sasldb/allockey.o
SASL_DB_INC = 
SASL_DB_LIB = 
SASL_DB_MANS = saslpasswd2.8 sasldblistusers2.8
SASL_DB_UTILS = saslpasswd2 sasldblistusers2
SASL_DL_LIB = 
SASL_KRB_LIB =  -lcrypto 
SASL_MECHS =  libsasldb.la libcrammd5.la libdigestmd5.la libotp.la libkerberos4.la libplain.la libanonymous.la
SASL_STATIC_LIBS = 
SASL_STATIC_OBJS =  ../plugins/sasldb.o ../plugins/cram.o ../plugins/digestmd5.o ../plugins/otp.o ../plugins/kerberos4.o ../plugins/plain.o ../plugins/anonymous.o ../plugins/plugin_common.o
SASL_UTIL_HEADERS_EXTRA = 
SASL_UTIL_LIBS_EXTRA = 
SFIO_INC_FLAGS = 
SFIO_LIB_FLAGS = 
SMTPTEST_PROGRAM = 
SNPRINTFOBJS = 
SRP_LIBS = 
VERSION = 2.1.1
plugindir = /usr/lib/sasl2

sbin_PROGRAMS = pwcheck

INCLUDES = -I../include -I../lib

pwcheck_SOURCES = pwcheck.c
EXTRA_pwcheck_SOURCES = pwcheck_getpwnam.c pwcheck_getspnam.c
pwcheck_DEPENDECIES = pwcheck_.lo
pwcheck_LDADD = pwcheck_.lo  
mkinstalldirs = $(SHELL) $(top_srcdir)/config/mkinstalldirs
CONFIG_HEADER = ../config.h
CONFIG_CLEAN_FILES = 
PROGRAMS =  $(sbin_PROGRAMS)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I..
CPPFLAGS = -I/usr/local/include 
LDFLAGS = -L/usr/local/lib 
LIBS = 
pwcheck_OBJECTS =  pwcheck.o
pwcheck_DEPENDENCIES =  pwcheck_.lo
pwcheck_LDFLAGS = 
CFLAGS = -Wall -W -g -O2
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
DIST_COMMON =  README Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = gtar
GZIP_ENV = --best
SOURCES = $(pwcheck_SOURCES) $(EXTRA_pwcheck_SOURCES)
OBJECTS = $(pwcheck_OBJECTS)

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .lo .o .s
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu --include-deps pwcheck/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-sbinPROGRAMS:

clean-sbinPROGRAMS:
	-test -z "$(sbin_PROGRAMS)" || rm -f $(sbin_PROGRAMS)

distclean-sbinPROGRAMS:

maintainer-clean-sbinPROGRAMS:

install-sbinPROGRAMS: $(sbin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	@list='$(sbin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo " $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	    $(LIBTOOL)  --mode=install $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	  else :; fi; \
	done

uninstall-sbinPROGRAMS:
	@$(NORMAL_UNINSTALL)
	list='$(sbin_PROGRAMS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(sbindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	done

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

.c.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.s.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

.S.lo:
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

mostlyclean-libtool:
	-rm -f *.lo

clean-libtool:
	-rm -rf .libs _libs

distclean-libtool:

maintainer-clean-libtool:

pwcheck: $(pwcheck_OBJECTS) $(pwcheck_DEPENDENCIES)
	@rm -f pwcheck
	$(LINK) $(pwcheck_LDFLAGS) $(pwcheck_OBJECTS) $(pwcheck_LDADD) $(LIBS)

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = pwcheck

distdir: $(DISTFILES)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am: install-sbinPROGRAMS
install-exec: install-exec-am

install-data-am:
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-sbinPROGRAMS
uninstall: uninstall-am
all-am: Makefile $(PROGRAMS)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(sbindir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-sbinPROGRAMS mostlyclean-compile \
		mostlyclean-libtool mostlyclean-tags \
		mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-sbinPROGRAMS clean-compile clean-libtool clean-tags \
		clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-sbinPROGRAMS distclean-compile \
		distclean-libtool distclean-tags distclean-generic \
		clean-am
	-rm -f libtool

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-sbinPROGRAMS \
		maintainer-clean-compile maintainer-clean-libtool \
		maintainer-clean-tags maintainer-clean-generic \
		distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-sbinPROGRAMS distclean-sbinPROGRAMS \
clean-sbinPROGRAMS maintainer-clean-sbinPROGRAMS uninstall-sbinPROGRAMS \
install-sbinPROGRAMS mostlyclean-compile distclean-compile \
clean-compile maintainer-clean-compile mostlyclean-libtool \
distclean-libtool clean-libtool maintainer-clean-libtool tags \
mostlyclean-tags distclean-tags clean-tags maintainer-clean-tags \
distdir info-am info dvi-am dvi check check-am installcheck-am \
installcheck install-exec-am install-exec install-data-am install-data \
install-am install uninstall-am uninstall all-redirect all-am all \
installdirs mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
