# Local additions to Autoconf macros.
# Copyright (C) 1992, 1994 Free Software Foundation, Inc.
# Francois Pinard <pinard@iro.umontreal.ca>, 1992.

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it defines @code{__STDC__} to 1 and
# handles function prototypes correctly.
# 
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{ac_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

define(fp_PROG_CC_STDC,
[AC_CACHE_CHECK(for ${CC-cc} option to accept ANSI C,
ac_cv_prog_cc_stdc,
[ac_cv_prog_cc_stdc=no
ac_save_CFLAGS="$CFLAGS"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" -Xc
do
  CFLAGS="$ac_save_CFLAGS $ac_arg"
  AC_TRY_COMPILE(
[#ifndef __STDC__
choke me
#endif	
], [int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[ac_cv_prog_cc_stdc="$ac_arg"; break])
done
CFLAGS="$ac_save_CFLAGS"
])
case "x$ac_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $ac_cv_prog_cc_stdc" ;;
esac
])

# Check for function prototypes.

AC_DEFUN(fp_C_PROTOTYPES,
[AC_REQUIRE([fp_PROG_CC_STDC])
AC_MSG_CHECKING([for function prototypes])
if test "$ac_cv_prog_cc_stdc" != no; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(PROTOTYPES)
  U=
else
  AC_MSG_RESULT(no)
  U=_
fi
AC_SUBST(U)dnl
])

dnl
dnl Code from the configure system for bash 2.03 (not zsh copyright).
dnl If available, use support for large files unless the user specified
dnl one of the CPPFLAGS, LDFLAGS, or LIBS variables (<eggert@twinsun.com>
dnl via GNU patch 2.5)
dnl
AC_DEFUN(zsh_LARGE_FILE_SUPPORT,
[AC_MSG_CHECKING(whether large file support needs explicit enabling)
ac_getconfs=''
ac_result=yes
ac_set=''
ac_shellvars='CPPFLAGS LDFLAGS LIBS'
for ac_shellvar in $ac_shellvars; do
  case $ac_shellvar in
  CPPFLAGS) ac_lfsvar=LFS_CFLAGS ;;
  *) ac_lfsvar=LFS_$ac_shellvar ;;
  esac
  eval test '"${'$ac_shellvar'+set}"' = set && ac_set=$ac_shellvar
  (getconf $ac_lfsvar) >/dev/null 2>&1 || { ac_result=no; break; }
  ac_getconf=`getconf $ac_lfsvar`
  ac_getconfs=$ac_getconfs$ac_getconf
  eval ac_test_$ac_shellvar="\$ac_getconf"
done
case "$ac_result$ac_getconfs" in
yes) ac_result=no ;;
esac
case "$ac_result$ac_set" in
yes?*) test "x$ac_set" != "xLDFLAGS" -o "x$auto_ldflags" = x && {
  ac_result="yes, but $ac_set is already set, so use its settings"
}
esac
AC_MSG_RESULT($ac_result)
case $ac_result in
yes)
  for ac_shellvar in $ac_shellvars; do
    case "`eval echo $ac_shellvar-\\\$ac_test_$ac_shellvar`" in
      CPPFLAGS*-D_LARGEFILE_SOURCE*) eval $ac_shellvar=\$ac_test_$ac_shellvar
	;;
      CPPFLAGS*) 
        eval $ac_shellvar="\"-D_LARGEFILE_SOURCE \$ac_test_$ac_shellvar\""
	;;
      *) eval $ac_shellvar=\$ac_test_$ac_shellvar
    esac
  done ;;
esac
])

dnl
dnl zsh_64_BIT_TYPE
dnl   Check whether the first argument works as a 64-bit type.
dnl   If there is a non-zero third argument, we just assume it works
dnl   when we're cross compiling.  This is to allow a type to be
dnl   specified directly as --enable-lfs="long long".
dnl   Sets the variable given in the second argument to the first argument
dnl   if the test worked, `no' otherwise.  Be careful testing this, as it
dnl   may produce two words `long long' on an unquoted substitution.
dnl   This macro does not produce messages as it may be run several times
dnl   before finding the right type.
dnl

AC_DEFUN(zsh_64_BIT_TYPE,
[AC_TRY_RUN([
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

main()
{
  $1 foo = 0; 
  return sizeof($1) != 8;
}
], $2="$1", $2=no,
  [if test x$3 != x ; then
    $2="$1"
  else
    $2=no
  fi])
])
