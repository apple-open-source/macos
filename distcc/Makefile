# Top-level Makefile(.in) for distcc

# Copyright (C) 2002, 2003 by Martin Pool

# Note that distcc no longer uses automake, but this file is still
# structured in a somewhat similar way.

# Remember that a CVS checkout of this project contains some
# directories that will not be present in a tarball distribution,
# including web/.  So, those directories must not be built by regular
# commands (make all, make clean, make distclean), only by
# maintainer-* or explicit invocations.

# TODO: Installing info files ought to add them to the info directory,
# but I don't know any portable way to do this, because it depends on
# editing a plain-text "dir" file.  Debian has "install-info", but
# that may not exist on all platforms that have GNU Info.

## VARIABLES

PACKAGE = distcc
VERSION = 2.0.1-zeroconf
PACKAGE_TARNAME = distcc
SHELL = /bin/sh

CFLAGS = -g -O2 -W -Wall -W -Wimplicit -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs -DDARWIN -D_REENTRANT -D__FreeBSD__
LDFLAGS = -lcrypto
CC = gcc
CPP = gcc -E
CPPFLAGS =  -DHAVE_CONFIG_H -D_GNU_SOURCE -I./popt -I./src

srcdir = .
top_srcdir = .

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
docdir = ${datadir}/doc
pkgdocdir = $(docdir)/distcc

LIBS = 

DESTDIR = $(DSTROOT)

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} 
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}

# You might need to override this depending on the name under which
# Python is installed here.
PYTHON = python2.3

dist_files = AUTHORS COPYING COPYING.FDL NEWS README		\
	DEPENDENCIES INSTALL README.packaging README.popt	\
	README.libiberty					\
	TODO							\
	packaging/SuSE/init.d/distcc				\
	survey.txt						\
	src/config.h.in						\
	$(dist_contrib)						\
	$(dist_patches)						\
	$(dist_common)						\
	$(MEN)							\
	$(pkgdoc_DOCS)						\
	$(popt_SRC) $(popt_HEADERS)				\
	$(SRC) $(HEADERS)					\
	$(test_SOURCE)						\
	$(bench_PY)

dist_contrib = contrib/distcc-absolutify	\
	contrib/distcc.sh			\
	contrib/distccd-init			\
	contrib/dmake				\
	contrib/make-j				\
	contrib/netpwd				\
	contrib/stage-cc-wrapper.patch

bench_PY = bench/Build.py bench/Project.py bench/ProjectDefs.py \
	bench/Summary.py bench/actions.py bench/benchmark.py	\
	bench/buildutil.py bench/compiler.py bench/statistics.py

pkgdoc_DOCS = AUTHORS COPYING COPYING.FDL NEWS README		\
	DEPENDENCIES INSTALL 

latte_HTML = web/compared.html web/doc.html web/download.html	\
	web/cvs.html web/faq.html web/index.html		\
	web/links.html web/news.html				\
	web/problems.html					\
	web/tested.html web/results.html web/roadmap.html	\
	web/scenarios.html web/old-news.html

mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
man1dir = $(mandir)/man1
man8dir = $(mandir)/man8
man1_MANS = man/distcc.1 man/distccd.1

# Contains HTML user manual
linuxdoc_DOCS = linuxdoc/distcc.ps.gz linuxdoc/distcc.pdf
linuxdoc_INFO = linuxdoc/distcc.info
manualdocdir = $(pkgdocdir)/manual

test_SOURCE = test/comfychair.py				\
	test/testdistcc.py			

dist_common = Makefile.in install-sh configure configure.ac \
	config.guess config.sub mkinstalldirs autogen.sh

# It seems a bit unnecessary to ship patches in the released tarballs.
# People who are so keen as to apply unsupported patches ought to use
# CVS, or at least get them from the list.
dist_patches = 

TAR = tar
GZIP = gzip
GZIP_OPT = -9v

BZIP2 = bzip2

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

distdir = $(PACKAGE_TARNAME)-$(VERSION)
tarball = $(PACKAGE_TARNAME)-$(VERSION).tar
tarball_bz2 = $(tarball).bz2
tarball_sig = $(tarball_bz2).asc
distnews = $(PACKAGE_TARNAME)-$(VERSION).NEWS

common_obj = src/trace.o src/util.o src/io.o src/exec.o src/arg.o	\
src/rpc.o src/tempfile.o src/bulk.o src/help.o src/filename.o		\
	src/lock.o src/ncpus.o						\
	src/where.o src/hosts.o src/sendfile.o				\
	src/snprintf.o src/timeval.o \
	src/indirect_client.o src/indirect_server.o src/indirect_util.o \
	src/zeroconf_client.o src/zeroconf_util.o

distcc_obj = src/clinet.o src/clirpc.o src/cpp.o src/distcc.o		\
	src/implicit.o src/strip.o $(common_obj)
# src/ssh.o

distccd_obj = src/access.o						\
	src/daemon.o src/dparent.o src/dsignal.o			\
	src/serve.o src/srvnet.o src/dopt.o				\
	src/setuid.o			 				\
	$(common_obj) $(popt_OBJS) \
	src/zeroconf_reg.o

distccschedd_obj = src/access.o                                         \
	src/dparent.o src/dsignal.o                                     \
	src/serve.o src/srvnet.o src/dopt.o                             \
	src/setuid.o                                                    \
	$(common_obj) $(popt_OBJS) \
	src/sched.o src/zeroconf_browse.o src/zeroconf_reg.o

h_exten_obj = src/h_exten.o src/trace.o src/util.o src/arg.o src/filename.o
h_issource_obj = src/h_issource.o src/trace.o src/util.o src/arg.o src/filename.o
h_scanargs_obj = src/h_scanargs.o src/trace.o src/util.o src/arg.o src/filename.o src/implicit.o
h_hosts_obj = src/h_hosts.o src/trace.o src/util.o src/hosts.o
h_argvtostr_obj = src/h_argvtostr.o src/trace.o src/util.o src/arg.o src/filename.o
h_strip_obj = src/h_strip.o src/trace.o src/util.o src/arg.o src/filename.o src/strip.o
h_parsemask_obj = src/h_parsemask.o $(common_obj) src/access.o

SRC = src/arg.c src/bulk.c src/clinet.c src/clirpc.c src/cpp.c		\
	src/daemon.c src/distcc.c src/dsignal.c				\
	src/ncpus.c							\
	src/sendfile.c src/ssh.c					\
	src/access.c							\
	src/dopt.c src/dparent.c src/exec.c src/filename.c		\
	src/h_argvtostr.c						\
	src/h_exten.c src/h_hosts.c src/h_issource.c src/h_parsemask.c	\
	src/h_scanargs.c						\
	src/help.c src/hosts.c src/io.c					\
	src/rpc.c src/serve.c src/snprintf.c				\
	src/srvnet.c src/tempfile.c src/timeval.c			\
	src/trace.c src/util.c src/where.c src/zip.c src/strip.c	\
	src/h_strip.c src/implicit.c src/lock.c src/setuid.c \
	src/indirect_client.c src/indirect_server.c src/indirect_util.c \
	src/zeroconf_browse.c src/zeroconf_client.c src/zeroconf_reg.c  \
	src/zeroconf_util.c src/sched.c

HEADERS = src/access.h src/bulk.h src/clinet.h src/clirpc.h src/cpp.h	\
	src/distcc.h src/dopt.h src/exitcode.h src/filename.h		\
	src/hosts.h src/implicit.h src/io.h				\
	src/rpc.h							\
	src/setuid.h src/snprintf.h src/strip.h				\
	src/tempfile.h src/timeval.h src/trace.h src/types.h		\
	src/util.h							\
	src/exec.h src/lock.h src/where.h src/srvnet.h \
	src/indirect_client.h src/indirect_server.h src/indirect_util.h \
	src/zeroconf_browse.h src/zeroconf_client.h src/zeroconf_reg.h  \
	src/zeroconf_util.h

MEN = man/distcc.1 man/distccd.1 man/distccschedd.1

popt_OBJS=popt/findme.o  popt/popt.o  popt/poptconfig.o \
	popt/popthelp.o popt/poptparse.o

popt_SRC=popt/findme.c  popt/popt.c  popt/poptconfig.c			 \
	popt/popthelp.c popt/poptparse.c

popt_HEADERS = popt/findme.h popt/popt.h popt/poptint.h popt/system.h


# You might think that distccd ought to be in sbin, because it's a
# daemon.  It is a grey area.  However, the Linux Filesystem Hierarchy
# Standard (FHS 2.2) says that sbin is for programs "used exclusively
# by the system administrator".  

# distccd will often be used by non-root users, and when we support
# ssh it will be somewhat important that it be found in their default
# path.  Therefore on balance it seems better to put it in bin/.  

# Package maintainers can override this if absolutely necessary, but I
# would prefer that they do not. -- mbp

bin_PROGRAMS = distcc distccd distccschedd

check_PROGRAMS = h_exten h_issource h_scanargs h_hosts h_argvtostr	\
	h_strip h_parsemask

## OVERALL targets

## IMPLICIT BUILD rules

.SUFFIXES: .html .latte .o .c

.latte.html:
	-rm -f $@
	if ! latte-html -l web/style.latte -o $@ $<; then rm $@; exit 1 ; fi

.c.o: 
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $< 

## OVERALL targets

## NOTE: "all" must be the first (default) rule, aside from patterns.

# We don't build the web pages or manual by default, because many
# people will not have the tools to do it.  Just use all-web or
# all-linuxdoc if you want them.
all: $(bin_PROGRAMS) $(sbin_PROGRAMS)

all-web: all-latte
all-latte: $(latte_HTML)

# TODO: perhaps ought to also build and upload a tarball of HTML files.

all-linuxdoc: linuxdoc/html/distcc.html $(linuxdoc_DOCS) $(linuxdoc_INFO)

## BUILD targets

distcc: $(distcc_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(distcc_obj) $(LIBS)

distccd: $(distccd_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(distccd_obj) $(LIBS)	

h_exten: $(h_exten_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_exten_obj) $(LIBS)

h_issource: $(h_issource_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_issource_obj) $(LIBS)

h_scanargs: $(h_scanargs_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_scanargs_obj) $(LIBS)

h_hosts: $(h_hosts_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_hosts_obj) $(LIBS)

h_argvtostr: $(h_argvtostr_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_argvtostr_obj) $(LIBS)

h_parsemask: $(h_parsemask_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_parsemask_obj) $(LIBS)

h_strip: $(h_strip_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(h_strip_obj) $(LIBS)

distccschedd: $(distccschedd_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(distccschedd_obj) $(LIBS)


## DIST targets

# The sub-targets copy (and if necessary, build) various files that
# have to go into the tarball.  They also create necessary directories
# -- bear in mind that they might be run in parallel.

# This looks a bit gross to me, but it's not as bad as it might be :-/

# I was going to try building a .bz2, but it turns out that for the
# current contents, it's not much better (~54% vs ~56%).  Probably
# this is because of the big compressed document files.

dist: 
	-rm -fr $(distdir)
	$(MAKE) dist-files dist-linuxdoc
	$(TAR) cfh $(tarball) $(distdir)
	$(BZIP2) -vf $(tarball)
	rm -r $(distdir)
	cp NEWS $(distnews)

distcheck: dist
	rm -rf '=distcheck'
	mkdir '=distcheck'
	cd '=distcheck' && bunzip2 < ../$(tarball_bz2) | $(TAR) xv && \
	cd $(distdir) && ./configure --prefix=`pwd`/prefix && \
	$(MAKE) && $(MAKE) install && $(MAKE) installcheck
	rm -rf '=distcheck'

dist-linuxdoc: linuxdoc/html/distcc.html linuxdoc/distcc.sgml
	mkdir -p $(distdir)/linuxdoc/html
	cp linuxdoc/distcc.sgml $(distdir)/linuxdoc
	cp linuxdoc/html/* $(distdir)/linuxdoc/html

dist-sign:
	gpg -a --detach-sign $(tarball_bz2)

# the sort function removes duplicates
dist-files: $(dist_files)
	for f in $(dist_files); do mkdir -p $(distdir)/`dirname $$f` || exit 1; \
	cp -a $(srcdir)/$$f $(distdir)/$$f || exit 1; done

## BUILD (linuxdoc) targets
linuxdoc/html/distcc.html: $(srcdir)/linuxdoc/distcc.sgml
	mkdir -p linuxdoc/html
	cd linuxdoc/html && linuxdoc --footer=../footer.html -B html ../distcc.sgml

linuxdoc/distcc.ps.gz: linuxdoc/distcc.sgml
	cd linuxdoc && linuxdoc -B latex -o ps -p a4 distcc.sgml 
	$(GZIP) $(GZIP_OPT) -f linuxdoc/distcc.ps

linuxdoc/distcc.pdf: linuxdoc/distcc.sgml
	cd linuxdoc && linuxdoc -B latex -o pdf -p a4 distcc.sgml

linuxdoc/distcc.info: linuxdoc/distcc.sgml
	cd linuxdoc && linuxdoc -B info distcc.sgml

## BUILD (web) targets
$(latte_HTML): web/style.latte


######################################################################
## CHECK targets

check_programs: $(check_PROGRAMS) $(bin_PROGRAMS) $(sbin_PROGRAMS)

check: check_programs
	if test x$(PYTHON) != x; then \
	$(PYTHON) -c 'import sys; print sys.version'; \
	PATH=`pwd`:$$PATH $(PYTHON) $(srcdir)/test/testdistcc.py; \
	else echo "WARNING: python not found; tests skipped"; \
	fi

# NB: This does not depend upon install; you might want to test another version.
installcheck: check_programs
	if test x$(PYTHON) != x; then \
	$(PYTHON) -c 'import sys; print sys.version'; \
	PATH="$(bindir):`pwd`:$$PATH" $(PYTHON) $(srcdir)/test/testdistcc.py; \
	else echo "WARNING: python not found; tests skipped"; \
	fi


######################################################################
## BENCHMARK targets
benchmark: 
	@echo "The distcc macro-benchmark uses your existing distcc installation"
	@if [ "$$DISTCC_HOSTS" ]; \
	then echo "DISTCC_HOSTS=\"$$DISTCC_HOSTS\""; \
	else echo "You must set up servers and set DISTCC_HOSTS before running the benchmark"; \
	exit 1; \
	fi
	@echo "This benchmark may download a lot of source files, and it takes a "
	@echo "long time to run.  Interrupt now if you want."
	@echo 
	@echo "Pass BENCH_ARGS to make to specify which benchmarks to run."
	@echo
	@sleep 5
	cd bench && $(PYTHON) benchmark.py $(BENCH_ARGS)


## CLEAN targets

clean: clean-autoconf
	rm -f src/*.o popt/*.o
	rm -f $(check_PROGRAMS) $(bin_PROGRAMS)
	rm -rf testtmp

clean-autoconf:
	rm -f config.cache config.log

maintainer-clean: distclean maintainer-clean-web maintainer-clean-linuxdoc \
	maintainer-clean-autoconf clean

# configure and co are distributed, but not in CVS
maintainer-clean-autoconf:
	rm -f configure src/config.h.in

maintainer-clean-web: 
	rm -f $(latte_HTML)

# We only remove the documentation files for maintainer-clean, because
# many users will not have the tools to rebuild them.  They're shipped
# up-to-date in the distribution.
maintainer-clean-linuxdoc: 
	cd linuxdoc && rm -f distcc.ps distcc.html distcc-*.html \
	html/* html/* distcc.info distcc.info.gz distcc.ps.gz distcc.pdf

distclean-autoconf:
	rm -f Makefile src/config.h config.status config.cache config.log

distclean: distclean-autoconf clean


## MAINTAINER targets

upload-web: all-web
	rsync -avz web/ survey.txt \
		--exclude CVS --exclude '*~' --exclude '*.latte' \
		samba.org:/home/httpd/distcc/

upload-linuxdoc: all-linuxdoc
	rsync -azv linuxdoc/html linuxdoc/distcc.pdf linuxdoc/distcc.ps.gz \
		--delete --exclude CVS --exclude '*~' \
		samba.org:/home/httpd/distcc/manual/

upload-dist:
	rsync -avP $(tarball) $(distnews) $(tarball_sig) samba.org:/home/ftp/pub/distcc/



### CVSPLOT

cvsplot:
	cvsplot -cvsdir . \
		-include '\.(c|h|py)$$' \
		-linedata /tmp/linedata.txt \
		-gnuplotlinedata web/cvslinedata.png \
		-filedata /tmp/filedata.txt \
		-gnuplotfiledata web/cvsfiledata.png



### INSTALL targets

# TODO: Allow root directory to be overridden for use in building
# packages.



showpaths:
	@echo "'make install' will install distcc as follows:"
	@echo "  man pages            $(DESTDIR)$(man1dir)"
	@echo "  documents            $(DESTDIR)$(pkgdocdir)"
	@echo "  programs             $(DESTDIR)$(bindir)"

# 	@echo "  info                 $(DESTDIR)$(infodir)"

# TODO: Perhaps don't rely on linuxdoc tools being present on a
# machine doing a build from CVS.

# install-sh can't handle multiple arguments, but we don't need any
# tricky features so mkinstalldirs and cp will do

#install: showpaths install-doc install-man install-programs try-install-linuxdoc
install: showpaths install-programs

installhdrs:
	@echo NO INSTALLHDRS

installsrc: $(SRCROOT)
	/bin/cp -R . $(SRCROOT)


install-programs: $(bin_PROGRAMS)
	/usr/bin/strip -x -S $^
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	for p in $(bin_PROGRAMS); do				\
	$(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir) || exit 1;	\
	done

install-man: $(man1_MANS)
	$(mkinstalldirs) $(DESTDIR)$(man1dir)
	for p in $(man1_MANS); do				\
	$(INSTALL_DATA)	$$p $(DESTDIR)$(man1dir) || exit 1;	\
	done

install-doc: $(pkgdoc_DOCS)
	$(mkinstalldirs) $(DESTDIR)$(pkgdocdir)
	for p in $(pkgdoc_DOCS); do				\
	$(INSTALL_DATA) $$p $(DESTDIR)$(pkgdocdir) || exit 1;	\
	done


# This target is primarily for people building from CVS.  If they
# don't have the tools to build the Linuxdoc manual from source, then
# the installation can still succeed with a warning.
try-install-linuxdoc: 
	-$(MAKE) install-linuxdoc

install-linuxdoc: linuxdoc/html/distcc.html
	$(mkinstalldirs) $(DESTDIR)$(manualdocdir)/html
	for p in linuxdoc/html/*; do				\
	$(INSTALL_DATA) $$p $(DESTDIR)$(manualdocdir)/html || exit 1; \
	done

# This is not run by default.
install-linuxdoc-info: $(linuxdoc_INFO)
	$(mkinstalldirs) $(DESTDIR)$(infodir)
	for p in $(linuxdoc_INFO); do				\
	$(INSTALL_DATA) $$p $(DESTDIR)$(infodir) || exit 1;	\
	done

## LINBOT targets

# This target checks the website for broken links.  It's probably not
# very interesting unless you are a maintainer and therefore in a
# position to fix them.

linbot-run:
	mkdir -p linbot-report
	linbot -o linbot-report http://distcc.samba.org/

## ANALOG targets

# These are probably only interesting for maintainers
analog-all:
	$(MAKE) analog-download
	$(MAKE) analog-resolve
	$(MAKE) analog-run

analog-download:
	rsync -avz 'samba.org:/var/log/httpd/distcc/*_log*' analog/

analog-run:
	mkdir -p analog/report && cd analog && analog -G +gdistcc.analog

analog-resolve:
	cat analog/access_log.* analog/access_log | \
		jdresolve --recursive --database analog/jdresolve.db - \
		--progress --timeout 15 --sockets 512 --linecache 100000 \
		> analog/resolved_log
