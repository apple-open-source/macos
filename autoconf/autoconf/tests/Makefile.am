## Process this file with automake to create Makefile.in. -*-Makefile-*-

## Makefile for Autoconf testsuite.
## Copyright (C) 2000, 2001, 2002 Free Software Foundation, Inc.

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

# We don't actually distribute the testsuite, since one only
# needs m4 to build it, m4 being required anyway to install Autoconf.
EXTRA_DIST = $(TESTSUITE_AT) atspecific.m4 mktests.sh \
             atlocal.in package.m4 wrapper.as

# Running the uninstalled scripts.
check_SCRIPTS = autoconf autoheader autoreconf autom4te autoscan autoupdate ifnames

DISTCLEANFILES = atconfig atlocal $(TESTSUITE) $(check_SCRIPTS)

# Import the dependencies on Autotest and M4sh.
include ../lib/freeze.mk


## ------------ ##
## package.m4.  ##
## ------------ ##

$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	{					\
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >$(srcdir)/package.m4



## ---------- ##
## Wrappers.  ##
## ---------- ##

$(srcdir)/wrapper.in: $(srcdir)/wrapper.as
	./autom4te --language=M4sh $(srcdir)/wrapper.as -o $@



## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_GENERATED_AT = \
	aclang.at acc.at acfortran.at \
	acgeneral.at acstatus.at \
	acautoheader.at acautoupdate.at \
	acspecific.at acfunctions.at acheaders.at actypes.at \
	aclibs.at acprograms.at

TESTSUITE_HAND_AT = \
	suite.at \
        m4sugar.at m4sh.at \
        base.at tools.at torture.at \
	compile.at c.at fortran.at \
	semantics.at \
	foreign.at

TESTSUITE_AT = $(TESTSUITE_GENERATED_AT) $(TESTSUITE_HAND_AT)
TESTSUITE = ./testsuite

# Run the non installed autom4te.
# Don't use AUTOM4TE since `make alpha' makes it unavailable although
# we are allowed to use it (since we ship it).
AUTOTEST = ./autom4te --language=autotest
$(TESTSUITE): $(srcdir)/package.m4 \
	      atspecific.m4 \
              $(TESTSUITE_AT) \
	      $(autotest_m4f_dependencies)
	$(AUTOTEST) -I $(srcdir) suite.at -o $@.tmp
	mv $@.tmp $@

atconfig: $(top_builddir)/config.status
	cd $(top_builddir) && ./config.status tests/$@

clean-local:
	test ! -f $(TESTSUITE) || $(SHELL) $(TESTSUITE) --clean
	rm -f *.tmp
	rm -rf autom4te.cache

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) $(TESTSUITE)

# Run the test suite on the *installed* tree.
installcheck-local:
	$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$(exec_prefix)/bin



## ------------------ ##
## Maintainer rules.  ##
## ------------------ ##

MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE_GENERATED_AT)

## Producing the test files.

# The files which contains macro we check for syntax.  Don't use $(top_srcdir)
# here since below we explicitly `cd' to $srcdir.  As for the dependencies,
# thanks God for VPATH.  Hm...
autoconfdir = $(top_srcdir)/lib/autoconf
AUTOCONF_FILES = $(autoconfdir)/general.m4 \
                 $(autoconfdir)/status.m4 \
                 $(autoconfdir)/autoheader.m4 \
                 $(autoconfdir)/autoupdate.m4 \
                 $(autoconfdir)/specific.m4 \
                 $(autoconfdir)/functions.m4 \
                 $(autoconfdir)/lang.m4 \
                 $(autoconfdir)/c.m4 \
                 $(autoconfdir)/fortran.m4 \
                 $(autoconfdir)/headers.m4 \
                 $(autoconfdir)/libs.m4 \
                 $(autoconfdir)/types.m4	\
                 $(autoconfdir)/programs.m4

$(TESTSUITE_GENERATED_AT): mktests.sh $(AUTOCONF_FILES)
	cd $(srcdir) && ./mktests.sh $(AUTOCONF_FILES)


## maintainer-check ##

maintainer-check: maintainer-check-posix maintainer-check-c++

# The hairy heredoc is more robust than using echo.
CLEANFILES = expr
expr:
	echo '#! $(SHELL)'            >expr
	echo 'result=`@EXPR@ "$$@"`' >>expr
	echo 'estatus=$$?'           >>expr
	echo 'cat <<EOF'             >>expr
	echo '$${result:-0}'         >>expr
	echo 'EOF'                   >>expr
	echo 'exit $$estatus'        >>expr
	chmod +x expr

# Try the test suite with more severe environments.
maintainer-check-posix: expr
	POSIXLY_CORRECTLY=yes make check
	rm expr

# Try using G++ as a C compiler.
maintainer-check-c++:
	CC=g++ make check
