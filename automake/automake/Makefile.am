## Process this file with automake to create Makefile.in

## Makefile for Automake.

## Copyright 1995, 1996, 1997, 1998, 1999, 2001 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

## We need `.' in SUBDIRS because we want `check' to build `.' before
## tests.
SUBDIRS = . m4 lib tests

bin_SCRIPTS = automake aclocal
info_TEXINFOS = automake.texi


TAGS_FILES = automake.in aclocal.in

# The following requires a fixed version of the Emacs 19.30 etags.
ETAGS_ARGS = --lang=none \
 --regex='/^@node[ \t]+\([^,]+\)/\1/' $(srcdir)/automake.texi
TAGS_DEPENDENCIES = automake.texi

EXTRA_DIST = ChangeLog.1996 ChangeLog.1998 ChangeLog.2000 ChangeLog.2001

## Make versioned links.  We only run the transform on the root name;
## then we make a versioned link with the transformed base name.  This
## seemed like the most reasonable approach.
install-exec-hook:
	@$(POST_INSTALL)
	@for p in $(bin_SCRIPTS); do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  fv="$$f-$(APIVERSION)"; \
	  rm -f $(DESTDIR)$(bindir)/$$fv; \
	  echo " $(LN) $(DESTDIR)$(bindir)/$$f $(DESTDIR)$(bindir)/$$fv"; \
	  $(LN) $(DESTDIR)$(bindir)/$$f $(DESTDIR)$(bindir)/$$fv; \
	done

uninstall-hook:
	@for p in $(bin_SCRIPTS); do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  fv="$$f-$(APIVERSION)"; \
	  rm -f $(DESTDIR)$(bindir)/$$fv; \
	done

################################################################
##
## Everything past here is useful to the maintainer, but probably not
## to anybody else
##

# Some simple checks, and then ordinary check.  These are only really
# guaranteed to work on my machine.
maintainer-check: automake aclocal
## This check avoids accidental configure substitutions in the source.
## There are exactly 7 lines that should be modified.  This works out
## to 22 lines of diffs.
	@if test `diff $(srcdir)/automake.in automake | wc -l` -ne 28; then \
	  echo "found too many diffs between automake.in and automake"; 1>&2; \
	  diff -c $(srcdir)/automake.in automake; \
	  exit 1; \
	fi
## Syntax check with default Perl (on my machine, Perl 5).
	perllibdir=$(srcdir)/lib $(PERL) -c -w automake
	perllibdir=$(srcdir)/lib $(PERL) -c -w aclocal
## expect no instances of '${...}'.  However, $${...} is ok, since that
## is a shell construct, not a Makefile construct.
## The backslash in `$${' is needed for some versions of bash.
	@if fgrep '\$${' $(srcdir)/lib/am/[a-z]*.am | \
	       fgrep -v '$$$$'; then \
	  echo "Found too many uses of '\$${' in the lines above." 1>&2; \
	  exit 1;				\
	else :; fi
## Make sure `rm' is called with `-f'.
	@if egrep '\<rm ([^-]|\-[^f ]*\>)' \
	          $(srcdir)/lib/am/[a-z]*.am $(srcdir)/tests/*.test | \
	      fgrep -v '##'; then \
	  echo "Suspicious 'rm' invocation." 1>&2; \
	  exit 1;				\
	else :; fi
## Never use something like `for file in $(FILES)', this doesn't work
## if FILES is empty or if it contains shell meta characters (e.g. $ is
## commonly used in Java filenames).
	@if egrep 'for .* in \$$\(' $(srcdir)/lib/am/[a-z]*.am; then \
	  echo 'Use "list=$$(mumble); for var in $$$$list".' 1>&2 ; \
	  exit 1; \
	else :; fi
## Make sure all invocations of mkinstalldirs are correct.
	@if fgrep -n 'mkinstalldirs' $(srcdir)/lib/am/[a-z]*.am | \
	      fgrep -v '$$(mkinstalldirs)'; then \
	  echo "Found incorrect use of mkinstalldirs in the lines above" 1>&2; \
	  exit 1; \
	else :; fi
## We never want to use "undef", only "delete", but for $/.
	@if grep -n -w 'undef ' $(srcdir)/automake.in | \
	      fgrep -v 'undef $$/'; then \
	  echo "Found undef in automake.in; use delete instead" 1>&2; \
	  exit 1; \
	fi
## We never want split (/ /,...), only split (' ', ...).
	@if fgrep -n 'split (/ /' $(srcdir)/automake.in; then \
	  echo "Found bad split in the lines above." 1>&2; \
	  exit 1; \
	fi
## Look for cd within backquotes
	@if grep -n '^[^#]*` *cd ' $(srcdir)/automake.in \
	      $(srcdir)/lib/am/*.am; then \
	  echo "Consider using $$$$(am__cd) in the line above." 1>&2; \
	  exit 1; \
	fi
## Using @_ in a scalar context is most probably a programming error.
	@if grep -Hn '[^) ] *= *@_' $(srcdir)/automake.in; then \
	  echo "Using @_ in a scalar context in the lines above." 1>&2; \
	  exit 1; \
	fi
## Forbid using parens with `local' to ease counting.
	@if grep '^[ \t]*local *(' $(srcdir)/automake.in; then \
	  echo "Don't use \`local' with parens: use several \`local' above." >&2; \
	  exit 1; \
	fi
## Up to now we manage to limit to 1 use of local, but for `local $_;'.
	@locals=`grep -v '^[ \t]*local \$$_;' $(srcdir)/automake.in | \
	        grep -c '^[ \t]*local [^*]'`; \
	case $$locals in \
	  [0] ) \
	    echo "Wow, congrats!  There are no \`local' now!." >&2; \
	    echo "Please update Makefile.am (maintainer-check)." >&2; \
	    exit 1; \
	  ;; \
	  1 ) ;; \
	  * ) \
	    echo "Too many \`local'!  Are you sure you need $$locals of them?" >&2; \
	    echo "Up to now 1 was enough." >&2; \
	    exit 1; \
	  ;; \
	esac
## Don't let AMDEP_TRUE substitution appear in automake.in.
	@if grep '@AMDEP''_TRUE@' $(srcdir)/automake.in; then \
	  echo "Don't put AMDEP_TRUE substitution in automake.in" 1>&2; \
	  exit 1; \
	fi
## Tests should never call make directly.
	@if grep -v '^#' $(srcdir)/tests/*.test | egrep ':[	]*make'; then \
	  echo 'Do not run "make" in the above tests.  Use "$$MAKE" instead.' 1>&2; \
	  exit 1; \
	fi
## Tests should never call autoconf directly.
	@if grep -v '^#' $(srcdir)/tests/*.test | egrep ':[	]*autoconf'; then \
	  echo 'Do not run "autoconf" in the above tests.  Use "$$AUTOCONF" instead.' 1>&2; \
	  exit 1; \
	fi
## Tests should never call automake directly.
	@if grep -v '^#' $(srcdir)/tests/*.test | egrep ':[	]*automake'; then \
	  echo 'Do not run "automake" in the above tests.  Use "$$AUTOMAKE" instead.' 1>&2;  \
	  exit 1; \
	fi
## Tests should never call aclocal directly.
	@if grep -v '^#' $(srcdir)/tests/*.test | egrep ':[	]*aclocal'; then \
	  echo 'Do not run "aclocal" in the above tests.  Use "$$ACLOCAL" instead.' 1>&2;  \
	  exit 1; \
	fi
## Tests should never call perl directly.
	@if grep -v '^#' $(srcdir)/tests/*.test | egrep ':[	]*perl'; then \
	  echo 'Do not run "perl" in the above tests.  Use "$$PERL" instead.' 1>&2; \
	  exit 1; \
	fi
## Overriding a Makefile macro on the command line is not portable when
## recursive targets are used.  Better use an envvar.  SHELL is an exception,
## POSIX says it can't come from the environment.
	@if egrep '\$$MAKE .*(SHELL=.*=|=.*SHELL=)' $(srcdir)/tests/*.test; then \
	  echo 'Rewrite "$$MAKE foo=bar SHELL=/bin/sh" as "foo=bar $$MAKE -e SHELL=/bin/sh"' 1>&2; \
	  echo ' in the above lines, it is more portable.' 1>&2; \
	  exit 1; \
	fi
	@if grep -v SHELL $(srcdir)/tests/*.test | grep '\$$MAKE .*=' ; then \
	  echo 'Rewrite "$$MAKE foo=bar" as "foo=bar $$MAKE -e SHELL=/bin/sh" in the above lines,' 1>&2; \
	  echo 'it is more portable.' 1>&2; \
	  exit 1; \
	fi
## On NetBSD (1.5) and OSF, the $SHELL variable is still inherited from
## the environment.
	@if grep '$$MAKE.* -e' $(srcdir)/tests/*.test | grep -v '-e.*SHELL' ; then \
	  echo 'Always overwrite SHELL when using "$$MAKE -e".' 1>&2; \
	  echo 'Use "$$MAKE -e SHELL=/bin/sh" in the above lines.' 1>&2; \
	  exit 1; \
	fi
	@if egrep 'SHELL=.*\$$MAKE' $(srcdir)/tests/*.test; then \
	  echo '$$MAKE ignores the SHELL envvar, use "$$MAKE SHELL=/bin/sh" in' 1>&2; \
	  echo 'the above lines.' 1>&2; \
	  exit 1; \
	fi
## Never use `sleep 1' to create files with different timestamps.
## Use `sleep 2' instead.  Some filesystems (e.g., Windows') have only
## a 2sec resolution.
	@if egrep '\bsleep +1\b' $(srcdir)/tests/*.test; then \
	  echo 'Do not use "sleep 1" in the above tests.  Use "sleep 2" instead.' 1>&2; \
	  exit 1; \
	fi


# Tag before making distribution.  Also, don't make a distribution if
# checks fail.  Also, make sure the NEWS file is up-to-date.
cvs-dist: maintainer-check distcheck
	@if sed 1q $(srcdir)/NEWS | grep -e "$(VERSION)" > /dev/null; then :; else \
	  echo "NEWS not updated; not releasing" 1>&2; \
	  exit 1;				\
	fi
	cd $(srcdir) && cvs -q tag `echo "Release-$(VERSION)" | sed 's/\./-/g'`
	$(MAKE) dist

cvs-diff:
	thisver=`echo "Release-$(VERSION)" | sed 's/\./-/g'`; \
	if test -z "$$OLDVERSION"; then \
	  prevno=`echo "$(VERSION)" - 0.01 | bc | sed 's/^\./0./'`; \
	else prevno="$$OLDVERSION"; fi; \
	prevver=Release-`echo $$prevno | sed 's/\./-/g'`; \
	cvs -f rdiff -c -r $$prevver -r $$thisver $(PACKAGE) \
	    > $(PACKAGE)-$$prevno-$(VERSION).diff

## Check our path lengths.
path-check: distdir
	(cd $(distdir) && \
## FIXME there's got to be a better way!  pathchk should take the list
## of files on stdin, at least.
	  find . -print | xargs pathchk -p); \
	  status=$$?; \
	  chmod -R a+w $(distdir) > /dev/null 2>&1; rm -rf $(distdir); \
	  exit $$status

## Program to use to fetch files.
WGET = wget

## Files that we fetch and which we compare against.
## FIXME should be a lot more here
FETCHFILES = config.guess config.sub texinfo.tex Automake/XFile.pm INSTALL \
	install-sh

## Fetch the latest versions of files we care about.
fetch:
	rm -rf Fetchdir > /dev/null 2>&1
	mkdir Fetchdir; mkdir Fetchdir/Automake
## If a get fails then that is a problem.
	(cd Fetchdir && \
	$(WGET) ftp://ftp.gnu.org/gnu/config/config.guess; \
	$(WGET) ftp://ftp.gnu.org/gnu/config/config.sub; \
	$(WGET) ftp://ftp.gnu.org/gnu/texinfo/texinfo.tex; \
	$(WGET) http://savannah.gnu.org/cgi-bin/viewcvs/~checkout~/autoconf/autoconf/lib/Autom4te/XFile.pm?content-type=text/plain -O Automake/XFile.pm; \
	$(WGET) http://savannah.gnu.org/cgi-bin/viewcvs/~checkout~/autoconf/autoconf/config/install-sh?content-type=text/plain -O install-sh; \
	$(WGET) http://savannah.gnu.org/cgi-bin/viewcvs/~checkout~/autoconf/autoconf/INSTALL?content-type=text/plain -O INSTALL)
	perl -pi -e 's/Autom4te::/Automake::/g' Fetchdir/Automake/XFile.pm
## Don't exit after test because we want to give as many errors as
## possible.
	@stat=0; for file in $(FETCHFILES); do \
	  if diff -u $(srcdir)/lib/$$file Fetchdir/$$file \
	          >>Fetchdir/update.patch 2>/dev/null; then :; \
	  else \
	    stat=1; \
	    echo "Updating $(srcdir)/lib/$$file..."; \
	    cp Fetchdir/$$file $(srcdir)/lib/$$file; \
	  fi; \
	done; \
	test $$stat = 1 && \
	  echo "See Fetchdir/update.patch for a log of the changes."; \
	exit $$stat
