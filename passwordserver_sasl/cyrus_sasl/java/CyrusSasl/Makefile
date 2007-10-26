# Makefile.in generated automatically by automake 1.4 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# Makefile.am for the Java SASL library
# Rob Earhart
#
################################################################
#        Copyright 1998 by Carnegie Mellon University
#
#                      All Rights Reserved
#
#Permission to use, copy, modify, and distribute this software and its
#documentation for any purpose and without fee is hereby granted,
#provided that the above copyright notice appear in all copies and that
#both that copyright notice and this permission notice appear in
#supporting documentation, and that the name of Carnegie Mellon University
#not be used in advertising or publicity pertaining to distribution of the
#software without specific, written prior permission.
#
#CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
#SOFTWARE, INCLUDING #ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, 
#IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR ANY SPECIAL, 
#INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
#LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
#OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#PERFORMANCE OF THIS SOFTWARE.
################################################################


SHELL = /bin/sh

srcdir = .
top_srcdir = ../..

prefix = /usr
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

top_builddir = ../..

ACLOCAL = ${SHELL} /Volumes/Forty/svn/passwordserver_sasl/cyrus_sasl/config/missing --run aclocal-1.7 -I $(top_srcdir)/cmulocal -I $(top_srcdir)/config
AUTOCONF = ${SHELL} /Volumes/Forty/svn/passwordserver_sasl/cyrus_sasl/config/missing --run autoconf
AUTOMAKE = ${SHELL} /Volumes/Forty/svn/passwordserver_sasl/cyrus_sasl/config/missing --run automake-1.7
AUTOHEADER = ${SHELL} /Volumes/Forty/svn/passwordserver_sasl/cyrus_sasl/config/missing --run autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = 
host_triplet = powerpc-apple-darwin9.0.0d4
AS = @AS@
AWK = awk
CC = gcc
CPP = gcc -E
DIRS = 
DLLTOOL = @DLLTOOL@
DMALLOC_LIBS = 
DYNSASLFLAGS = @DYNSASLFLAGS@
GETADDRINFOOBJS = 
GETNAMEINFOOBJS = 
GETSUBOPT = 
GSSAPIBASE_LIBS =  -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err -lresolv
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
LIB_PAM = @LIB_PAM@
LIB_SASL = @LIB_SASL@
LIB_SOCKET = 
LN_S = ln -s
LTGETADDRINFOOBJS = 
LTGETNAMEINFOOBJS = 
LTLIBOBJS = 
LTSNPRINTFOBJS = 
MAKEINFO = ${SHELL} /Volumes/Forty/svn/passwordserver_sasl/cyrus_sasl/config/missing --run makeinfo
NM = /usr/bin/nm -p
OBJDUMP = @OBJDUMP@
OTP_LIBS = -lcrypto 
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
SASL_DL_LIB = -ldl
SASL_KRB_LIB = 
SASL_MECHS =  libsasldb.la libcrammd5.la libdigestmd5.la libotp.la libgssapiv2.la libplain.la libanonymous.la
SASL_STATIC_LIBS = 
SASL_STATIC_OBJS =  ../plugins/gssapi.o
SASL_UTIL_HEADERS_EXTRA = 
SASL_UTIL_LIBS_EXTRA = 
SFIO_INC_FLAGS = 
SFIO_LIB_FLAGS = 
SMTPTEST_PROGRAM = 
SNPRINTFOBJS = 
SRP_LIBS = 
VERSION = 2.1.18
plugindir = /usr/lib/sasl2

javasasl_version = 1:0:0

javasasldir = $(prefix)/lib/java/classes/sasl/CyrusSasl
javahtmldir = $(prefix)/html/sasl

INCLUDES = -I$(top_srcdir)/include $(JAVA_INCLUDES)

javasasl_JAVA = Sasl.java GenericClient.java 		ClientFactory.java 	        GenericCommon.java SaslClient.java 	        SaslClientFactory.java SaslException.java 	        SaslInputStream.java SaslOutputStream.java 		SaslUtils.java ServerFactory.java                 SaslServerFactory.java SaslServer.java                 GenericServer.java


CLASSES = $(javasasl_JAVA:.java=.class)

lib_LTLIBRARIES = libjavasasl.la
libjavasasl_la_SOURCES = javasasl.h javasasl.c
libjavasasl_la_LDFLAGS = -export_dynamic -L../../lib -lsasl2 -version-info $(javasasl_version) $(wildcard ../lib/*.lo)
BUILT_SOURCES = javasasl.h $(CLASSES)
mkinstalldirs = $(SHELL) $(top_srcdir)/config/mkinstalldirs
CONFIG_HEADER = ../../config.h
CONFIG_CLEAN_FILES = 
LTLIBRARIES =  $(lib_LTLIBRARIES)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I../..
CPPFLAGS = 
LDFLAGS = 
LIBS = -lresolv -lresolv 
libjavasasl_la_LIBADD = 
libjavasasl_la_OBJECTS =  javasasl.lo
CFLAGS = -Wall -W -g -O2
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
LTCOMPILE = $(LIBTOOL) --mode=compile $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(LIBTOOL) --mode=link $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
JAVACFLAGS = 
CLASSPATH_ENV = CLASSPATH=$(JAVAROOT):$(srcdir)/$(JAVAROOT):$$CLASSPATH
DIST_COMMON =  $(javasasl_JAVA) Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = gtar
GZIP_ENV = --best
SOURCES = $(libjavasasl_la_SOURCES)
OBJECTS = $(libjavasasl_la_OBJECTS)

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .lo .o .s
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu --include-deps java/CyrusSasl/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-libLTLIBRARIES:

clean-libLTLIBRARIES:
	-test -z "$(lib_LTLIBRARIES)" || rm -f $(lib_LTLIBRARIES)

distclean-libLTLIBRARIES:

maintainer-clean-libLTLIBRARIES:

install-libLTLIBRARIES: $(lib_LTLIBRARIES)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	@list='$(lib_LTLIBRARIES)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "$(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(libdir)/$$p"; \
	    $(LIBTOOL)  --mode=install $(INSTALL) $$p $(DESTDIR)$(libdir)/$$p; \
	  else :; fi; \
	done

uninstall-libLTLIBRARIES:
	@$(NORMAL_UNINSTALL)
	list='$(lib_LTLIBRARIES)'; for p in $$list; do \
	  $(LIBTOOL)  --mode=uninstall rm -f $(DESTDIR)$(libdir)/$$p; \
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

libjavasasl.la: $(libjavasasl_la_OBJECTS) $(libjavasasl_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libjavasasl_la_LDFLAGS) $(libjavasasl_la_OBJECTS) $(libjavasasl_la_LIBADD) $(LIBS)

mostlyclean-javasaslJAVA:

clean-javasaslJAVA:
	-rm *.class classjavasasl.stamp

distclean-javasaslJAVA:

maintainer-clean-javasaslJAVA:

install-javasaslJAVA: classjavasasl.stamp
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(javasasldir)
	for p in *.class; do \
	  echo " $(INSTALL_DATA) $$p $(DESTDIR)$(javasasldir)/$$p"; \
	  $(INSTALL_DATA) $$p $(DESTDIR)$(javasasldir)/$$p; \
	done

uninstall-javasaslJAVA:
	@$(NORMAL_UNINSTALL)
	for p in *.class; do \
	  rm -f $(DESTDIR)$(javasasldir)/$$p; \
	done
classjavasasl.stamp: $(javasasl_JAVA)
	$(CLASSPATH_ENV) $(JAVAC) -d $(JAVAROOT) $(JAVACFLAGS) $?
	echo timestamp > classjavasasl.stamp

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

subdir = java/CyrusSasl

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
install-exec-am: install-libLTLIBRARIES
install-exec: install-exec-am

install-data-am: install-javasaslJAVA
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-libLTLIBRARIES uninstall-javasaslJAVA
uninstall: uninstall-am
all-am: Makefile $(LTLIBRARIES) classjavasasl.stamp
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(libdir) $(DESTDIR)$(javasasldir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
	-test -z "$(BUILT_SOURCES)" || rm -f $(BUILT_SOURCES)
mostlyclean-am:  mostlyclean-libLTLIBRARIES mostlyclean-compile \
		mostlyclean-libtool mostlyclean-javasaslJAVA \
		mostlyclean-tags mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-libLTLIBRARIES clean-compile clean-libtool \
		clean-javasaslJAVA clean-tags clean-generic \
		mostlyclean-am

clean: clean-am

distclean-am:  distclean-libLTLIBRARIES distclean-compile \
		distclean-libtool distclean-javasaslJAVA distclean-tags \
		distclean-generic clean-am
	-rm -f libtool

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-libLTLIBRARIES \
		maintainer-clean-compile maintainer-clean-libtool \
		maintainer-clean-javasaslJAVA maintainer-clean-tags \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-libLTLIBRARIES distclean-libLTLIBRARIES \
clean-libLTLIBRARIES maintainer-clean-libLTLIBRARIES \
uninstall-libLTLIBRARIES install-libLTLIBRARIES mostlyclean-compile \
distclean-compile clean-compile maintainer-clean-compile \
mostlyclean-libtool distclean-libtool clean-libtool \
maintainer-clean-libtool mostlyclean-javasaslJAVA \
distclean-javasaslJAVA clean-javasaslJAVA maintainer-clean-javasaslJAVA \
uninstall-javasaslJAVA install-javasaslJAVA tags mostlyclean-tags \
distclean-tags clean-tags maintainer-clean-tags distdir info-am info \
dvi-am dvi check check-am installcheck-am installcheck install-exec-am \
install-exec install-data-am install-data install-am install \
uninstall-am uninstall all-redirect all-am all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


$(srcdir)/javasasl.c: javasasl.h

javasasl.h: $(CLASSES)
	$(CLASSPATH_ENV) $(JAVAH) -o $@ -jni $(patsubst %.class,CyrusSasl.%,$^)

# force build of class files
$(CLASSES): classjavasasl.stamp

#install-data-local:
# xxx broken
#	$(mkinstalldirs) $(javahtmldir)
#	$(CLASSPATH_ENV) $(JAVADOC) -d $(javahtmldir) sasl
#	-if test ! -h $(javahtmldir)/images; \
#	then \
#	  $(LN_S) $(JAVA_BASE)/docs/api/images $(javahtmldir)/images; \
#	fi

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
