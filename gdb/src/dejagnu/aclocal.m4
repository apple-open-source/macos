dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

AC_DEFUN(DJ_AC_STL, [
AC_MSG_CHECKING(for STL versions)
AC_CACHE_VAL(ac_cv_stl,[
  AC_LANG_CPLUSPLUS
  AC_TRY_COMPILE([#include <iostream>], [
  using namespace std;
  char bbuuff[5120];
  cout.rdbuf()->pubsetbuf(bbuuff, 5120); ],
  ac_cv_stl=v3
  ,
  ac_cv_stl=v2
  ),
])

AC_LANG_C
if test x"${ac_cv_stl}" != x"v2" ; then  
  AC_MSG_RESULT(v3)
  AC_DEFINE(HAVE_STL3)
else
  AC_MSG_RESULT(v2)
fi
])

AC_DEFUN(DJ_AC_PATH_TCLSH, [
dirlist=".. ../../ ../../../ ../../../../ ../../../../../ ../../../../../../ ../
../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../..
/../../../../../.."
no_itcl=true
AC_MSG_CHECKING(for the tclsh program)
AC_ARG_WITH(tclinclude, [  --with-tclinclude       directory where tcl header
s are], with_tclinclude=${withval})
AC_CACHE_VAL(ac_cv_path_tclsh,[
dnl first check to see if --with-itclinclude was specified
if test x"${with_tclinclude}" != x ; then
  if test -f ${with_tclinclude}/tclsh ; then
    ac_cv_path_tclsh=`(cd ${with_tclinclude}; pwd)`
  elif test -f ${with_tclinclude}/src/tclsh ; then
    ac_cv_path_tclsh=`(cd ${with_tclinclude}/src; pwd)`
  else
    AC_MSG_ERROR([${with_tclinclude} directory doesn't contain tclsh])
  fi
fi
])

dnl next check in private source directory
dnl since ls returns lowest version numbers first, reverse its output
if test x"${ac_cv_path_tclsh}" = x ; then
    dnl find the top level Itcl source directory
    for i in $dirlist; do
        if test -n "`ls -dr $srcdir/$i/tcl* 2>/dev/null`" ; then
            tclpath=$srcdir/$i
            break
        fi
    done

    dnl find the exact Itcl source dir. We do it this way, cause there
    dnl might be multiple version of Itcl, and we want the most recent one.
    for i in `ls -dr $tclpath/tcl* 2>/dev/null ` ; do
        if test -f $i/src/tclsh ; then
          ac_cv_path_tclsh=`(cd $i/src; pwd)`/tclsh
          break
        fi
    done
fi

dnl see if one is installed
if test x"${ac_cv_path_tclsh}" = x ; then
   AC_MSG_RESULT(none)
   AC_PATH_PROG(tclsh, tclsh)
else
   AC_MSG_RESULT(${ac_cv_path_tclsh})
fi
TCLSH="${ac_cv_path_tclsh}"
AC_SUBST(TCLSH)
])


AC_DEFUN(DJ_AC_PATH_DOCBOOK, [
dirlist=".. ../../ ../../.. ../../../.. ../../../../.. ../../../../../.. ../../../../../../.. ../../../../../../../.. ../../../../../../../../.. ../../../../../../../../../.."
AC_MSG_CHECKING(for docbook tools)
AC_ARG_WITH(oskith, [  --with-docbook       directory where the db2 sgml tools are], with_docbook=${withval})
AC_CACHE_VAL(ac_cv_c_docbook,[
dnl first check to see if --with-docbook was specified
if test x"${with_docbook}" != x ; then
  if test -f ${with_docbook}/db2html ; then
    ac_cv_c_docbook=`(cd ${with_docbook}; pwd)`
  else
    AC_MSG_ERROR([${with_docbook} directory doesn't contain SGML tools])
  fi
fi
])
if test x"${ac_cv_c_docbook}" = x ; then
    for i in $ac_default_prefix/bin /usr/local/bin $OSKITHDIR/../bin /usr/bin /bin /opt /home; do
	dnl See is we have an SGML tool in that directory.
	if test -f $i/db2html ; then
	    ac_cv_c_docbook=$i
	    break
	fi
    done
fi

if test x"${ac_cv_c_docbook}" = x ; then
    AC_MSG_RESULT(none)
else
    DOCBOOK="${ac_cv_c_docbook}"
    AC_MSG_RESULT(${ac_cv_c_docbook})
fi

AC_SUBST(DOCBOOK)
])

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 1

dnl Usage:
dnl AM_INIT_AUTOMAKE(package,version, [no-define])

AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AC_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
dnl test to see if srcdir already configured
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Version number of package]))
AC_REQUIRE([AM_SANITY_CHECK])
AC_REQUIRE([AC_ARG_PROGRAM])
dnl FIXME This is truly gross.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_REQUIRE([AC_PROG_MAKE_SET])])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "[$]*" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   if test "[$]*" != "X $srcdir/configure conftestfile" \
      && test "[$]*" != "X conftestfile $srcdir/configure"; then

      # If neither matched, then we have a broken ls.  This can happen
      # if, for instance, CONFIG_SHELL is bash and it inherits a
      # broken ls alias from the environment.  This has actually
      # happened.  Such a system could not be considered "sane".
      AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
alias in your environment])
   fi

   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
dnl The program must properly implement --version.
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf.  Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])

# Add --enable-maintainer-mode option to configure.
# From Jim Meyering

# serial 1

AC_DEFUN(AM_MAINTAINER_MODE,
[AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode is disabled by default
  AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode enable make rules and dependencies not useful
                          (and sometimes confusing) to the casual installer],
      USE_MAINTAINER_MODE=$enableval,
      USE_MAINTAINER_MODE=no)
  AC_MSG_RESULT($USE_MAINTAINER_MODE)
  AM_CONDITIONAL(MAINTAINER_MODE, test $USE_MAINTAINER_MODE = yes)
  MAINT=$MAINTAINER_MODE_TRUE
  AC_SUBST(MAINT)dnl
]
)

# Define a conditional.

AC_DEFUN(AM_CONDITIONAL,
[AC_SUBST($1_TRUE)
AC_SUBST($1_FALSE)
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi])


dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, "$missing_dir/missing flex")
AC_PROG_LEX
AC_DECL_YYTEXT])

