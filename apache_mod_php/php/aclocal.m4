dnl aclocal.m4 generated automatically by aclocal 1.4

dnl Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY, to the extent permitted by law; without
dnl even the implied warranty of MERCHANTABILITY or FITNESS FOR A
dnl PARTICULAR PURPOSE.

dnl $Id: aclocal.m4,v 1.1.1.3 2001/01/25 04:58:47 wsanchez Exp $
dnl
dnl This file contains local autoconf functions.

sinclude(dynlib.m4)

dnl PHP_EVAL_LIBLINE(LINE, SHARED-LIBADD)
dnl
dnl Use this macro, if you need to add libraries and or library search
dnl paths to the PHP build system which are only given in compiler
dnl notation.
dnl
AC_DEFUN(PHP_EVAL_LIBLINE,[
  for ac_i in $1; do
    case "$ac_i" in
    -l*)
      ac_ii=`echo $ac_i|cut -c 3-`
      AC_ADD_LIBRARY($ac_ii,,$2)
    ;;
    -L*)
      ac_ii=`echo $ac_i|cut -c 3-`
      AC_ADD_LIBPATH($ac_ii,$2)
    ;;
    esac
  done
])

dnl PHP_EVAL_INCLINE(LINE)
dnl
dnl Use this macro, if you need to add header search paths to the PHP
dnl build system which are only given in compiler notation.
dnl
AC_DEFUN(PHP_EVAL_INCLINE,[
  for ac_i in $1; do
    case "$ac_i" in
    -I*)
      ac_ii=`echo $ac_i|cut -c 3-`
      AC_ADD_INCLUDE($ac_ii)
    ;;
    esac
  done
])
	
AC_DEFUN(PHP_READDIR_R_TYPE,[
  dnl HAVE_READDIR_R is also defined by libmysql
  AC_CHECK_FUNC(readdir_r,ac_cv_func_readdir_r=yes,ac_cv_func_readdir=no)
  if test "$ac_cv_func_readdir_r" = "yes"; then
  AC_CACHE_CHECK(for type of readdir_r, ac_cv_what_readdir_r,[
    AC_TRY_RUN([
#define _REENTRANT
#include <sys/types.h>
#include <dirent.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

main() {
	DIR *dir;
	char entry[sizeof(struct dirent)+PATH_MAX];
	struct dirent *pentry = (struct dirent *) &entry;

	dir = opendir("/");
	if (!dir) 
		exit(1);
	if (readdir_r(dir, (struct dirent *) entry, &pentry) == 0)
		exit(0);
	exit(1);
}
    ],[
      ac_cv_what_readdir_r=POSIX
    ],[
      AC_TRY_CPP([
#define _REENTRANT
#include <sys/types.h>
#include <dirent.h>
int readdir_r(DIR *, struct dirent *);
        ],[
          ac_cv_what_readdir_r=old-style
        ],[
          ac_cv_what_readdir_r=none
      ])
    ],[
      ac_cv_what_readdir_r=none
   ])
  ])
    case "$ac_cv_what_readdir_r" in
    POSIX)
      AC_DEFINE(HAVE_POSIX_READDIR_R,1,[whether you have POSIX readdir_r]);;
    old-style)
      AC_DEFINE(HAVE_OLD_READDIR_R,1,[whether you have old-style readdir_r]);;
    esac
  fi
])

AC_DEFUN(PHP_SHLIB_SUFFIX_NAME,[
  PHP_SUBST(SHLIB_SUFFIX_NAME)
  SHLIB_SUFFIX_NAME=so
  case "$host_alias" in
  *hpux*)
	SHLIB_SUFFIX_NAME=sl
	;;
  esac
])

AC_DEFUN(PHP_DEBUG_MACRO,[
  DEBUG_LOG="$1"
  cat >$1 <<X
CONFIGURE:  $CONFIGURE_COMMAND
CC:         $CC
CFLAGS:     $CFLAGS
CPPFLAGS:   $CPPFLAGS
CXX:        $CXX
CXXFLAGS:   $CXXFLAGS
INCLUDES:   $INCLUDES
LDFLAGS:    $LDFLAGS
LIBS:       $LIBS
DLIBS:      $DLIBS
SAPI:       $PHP_SAPI
PHP_RPATHS: $PHP_RPATHS
uname -a:   `uname -a`

X
    cat >conftest.$ac_ext <<X
main()
{
  exit(0);
}
X
    (eval echo \"$ac_link\"; eval $ac_link && ./conftest) >>$1 2>&1
    rm -fr conftest*
])
	
AC_DEFUN(PHP_MISSING_TIME_R_DECL,[
  AC_MSG_CHECKING(for missing declarations of reentrant functions)
  AC_TRY_COMPILE([#include <time.h>],[struct tm *(*func)() = localtime_r],[
    :
  ],[
    AC_DEFINE(MISSING_LOCALTIME_R_DECL,1,[Whether localtime_r is declared])
  ])
  AC_TRY_COMPILE([#include <time.h>],[struct tm *(*func)() = gmtime_r],[
    :
  ],[
    AC_DEFINE(MISSING_GMTIME_R_DECL,1,[Whether gmtime_r is declared])
  ])
  AC_TRY_COMPILE([#include <time.h>],[char *(*func)() = asctime_r],[
    :
  ],[
    AC_DEFINE(MISSING_ASCTIME_R_DECL,1,[Whether asctime_r is declared])
  ])
  AC_TRY_COMPILE([#include <time.h>],[char *(*func)() = ctime_r],[
    :
  ],[
    AC_DEFINE(MISSING_CTIME_R_DECL,1,[Whether ctime_r is declared])
  ])
  AC_TRY_COMPILE([#include <string.h>],[char *(*func)() = strtok_r],[
    :
  ],[
    AC_DEFINE(MISSING_STRTOK_R_DECL,1,[Whether strtok_r is declared])
  ])
  AC_MSG_RESULT(done)
])

dnl
dnl PHP_LIBGCC_LIBPATH(gcc)
dnl Stores the location of libgcc in libgcc_libpath
dnl
AC_DEFUN(PHP_LIBGCC_LIBPATH,[
  changequote({,})
  libgcc_libpath="`$1 --print-libgcc-file-name|sed 's%/*[^/][^/]*$%%'`"
  changequote([,])
])

AC_DEFUN(PHP_ARG_ANALYZE,[
case "[$]$1" in
shared,*)
  ext_output="yes, shared"
  ext_shared=yes
  $1=`echo $ac_n "[$]$1$ac_c"|sed s/^shared,//`
  ;;
shared)
  ext_output="yes, shared"
  ext_shared=yes
  $1=yes
  ;;
no)
  ext_output="no"
  ext_shared=no
  ;;
*)
  ext_output="yes"
  ext_shared=no
  ;;
esac

if test "$php_always_shared" = "yes"; then
  ext_output="yes, shared"
  ext_shared=yes
  test "[$]$1" = "no" && $1=yes
fi

AC_MSG_RESULT($ext_output)
])

dnl
dnl PHP_ARG_WITH(arg-name, check message, help text[, default-val])
dnl Sets PHP_ARG_NAME either to the user value or to the default value.
dnl default-val defaults to no. 
dnl
AC_DEFUN(PHP_ARG_WITH,[
PHP_REAL_ARG_WITH([$1],[$2],[$3],[$4],PHP_[]translit($1,a-z0-9-,A-Z0-9_))
])

AC_DEFUN(PHP_REAL_ARG_WITH,[
AC_MSG_CHECKING($2)
AC_ARG_WITH($1,[$3],$5=[$]withval,$5=ifelse($4,,no,$4))
PHP_ARG_ANALYZE($5)
])

dnl
dnl PHP_ARG_ENABLE(arg-name, check message, help text[, default-val])
dnl Sets PHP_ARG_NAME either to the user value or to the default value.
dnl default-val defaults to no. 
dnl
AC_DEFUN(PHP_ARG_ENABLE,[
PHP_REAL_ARG_ENABLE([$1],[$2],[$3],[$4],PHP_[]translit($1,a-z-,A-Z_))
])

AC_DEFUN(PHP_REAL_ARG_ENABLE,[
AC_MSG_CHECKING($2)
AC_ARG_ENABLE($1,[$3],$5=[$]enableval,$5=ifelse($4,,no,$4))
PHP_ARG_ANALYZE($5)
])

AC_DEFUN(PHP_MODULE_PTR,[
  EXTRA_MODULE_PTRS="$EXTRA_MODULE_PTRS $1,"
])
 
AC_DEFUN(PHP_CONFIG_NICE,[
  rm -f $1
  cat >$1<<EOF
#! /bin/sh
#
# Created by configure

EOF

  for arg in [$]0 "[$]@"; do
    echo "\"[$]arg\" \\" >> $1
  done
  echo '"[$]@"' >> $1
  chmod +x $1
])

AC_DEFUN(PHP_TIME_R_TYPE,[
AC_CACHE_CHECK(for type of reentrant time-related functions, ac_cv_time_r_type,[
AC_TRY_RUN([
#include <time.h>
#include <stdlib.h>

main() {
char buf[27];
struct tm t;
time_t old = 0;
int r, s;

s = gmtime_r(&old, &t);
r = (int) asctime_r(&t, buf, 26);
if (r == s && s == 0) exit(0);
exit(1);
}
],[
  ac_cv_time_r_type=hpux
],[
  ac_cv_time_r_type=POSIX
],[
  ac_cv_time_r_type=POSIX
])
])
if test "$ac_cv_time_r_type" = "hpux"; then
  AC_DEFINE(PHP_HPUX_TIME_R,1,[Whether you have HP-UX 10.x])
fi
])

AC_DEFUN(PHP_SUBST,[
  PHP_VAR_SUBST="$PHP_VAR_SUBST $1"
  AC_SUBST($1)
])

AC_DEFUN(PHP_FAST_OUTPUT,[
  PHP_FAST_OUTPUT_FILES="$PHP_FAST_OUTPUT_FILES $1"
])

AC_DEFUN(PHP_MKDIR_P_CHECK,[
  AC_CACHE_CHECK(for working mkdir -p, ac_cv_mkdir_p,[
    test -d conftestdir && rm -rf conftestdir
    mkdir -p conftestdir/somedir >/dev/null 2>&1
    if test -d conftestdir/somedir; then
      ac_cv_mkdir_p=yes
    else
      ac_cv_mkdir_p=no
    fi
    rm -rf conftestdir
  ])
])

AC_DEFUN(PHP_GEN_CONFIG_VARS,[
  PHP_MKDIR_P_CHECK
  echo creating config_vars.mk
  > config_vars.mk
  for i in $PHP_VAR_SUBST; do
    eval echo "$i = \$$i" >> config_vars.mk
  done
])

AC_DEFUN(PHP_GEN_MAKEFILES,[
  $SHELL $srcdir/build/fastgen.sh $srcdir $ac_cv_mkdir_p $BSD_MAKEFILE $1
])

AC_DEFUN(PHP_TM_GMTOFF,[
AC_CACHE_CHECK([for tm_gmtoff in struct tm], ac_cv_struct_tm_gmtoff,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <$ac_cv_struct_tm>], [struct tm tm; tm.tm_gmtoff;],
  ac_cv_struct_tm_gmtoff=yes, ac_cv_struct_tm_gmtoff=no)])

if test "$ac_cv_struct_tm_gmtoff" = yes; then
  AC_DEFINE(HAVE_TM_GMTOFF,1,[whether you have tm_gmtoff in struct tm])
fi
])

dnl PHP_CONFIGURE_PART(MESSAGE)
dnl Idea borrowed from mm
AC_DEFUN(PHP_CONFIGURE_PART,[
  AC_MSG_RESULT()
  AC_MSG_RESULT(${T_MD}$1${T_ME})
])

AC_DEFUN(PHP_PROG_SENDMAIL,[
AC_PATH_PROG(PROG_SENDMAIL, sendmail, /usr/lib/sendmail, $PATH:/usr/bin:/usr/sbin:/usr/etc:/etc:/usr/ucblib)
if test -n "$PROG_SENDMAIL"; then
  AC_DEFINE(HAVE_SENDMAIL,1,[whether you have sendmail])
fi
])

AC_DEFUN(PHP_RUNPATH_SWITCH,[
dnl check for -R, etc. switch
AC_MSG_CHECKING(if compiler supports -R)
AC_CACHE_VAL(php_cv_cc_dashr,[
	SAVE_LIBS="${LIBS}"
	LIBS="-R /usr/lib ${LIBS}"
	AC_TRY_LINK([], [], php_cv_cc_dashr=yes, php_cv_cc_dashr=no)
	LIBS="${SAVE_LIBS}"])
AC_MSG_RESULT($php_cv_cc_dashr)
if test $php_cv_cc_dashr = "yes"; then
	ld_runpath_switch="-R"
else
	AC_MSG_CHECKING([if compiler supports -Wl,-rpath,])
	AC_CACHE_VAL(php_cv_cc_rpath,[
		SAVE_LIBS="${LIBS}"
		LIBS="-Wl,-rpath,/usr/lib ${LIBS}"
		AC_TRY_LINK([], [], php_cv_cc_rpath=yes, php_cv_cc_rpath=no)
		LIBS="${SAVE_LIBS}"])
	AC_MSG_RESULT($php_cv_cc_rpath)
	if test $php_cv_cc_rpath = "yes"; then
		ld_runpath_switch="-Wl,-rpath,"
	else
		dnl something innocuous
		ld_runpath_switch="-L"
	fi
fi
])

AC_DEFUN(PHP_STRUCT_FLOCK,[
AC_CACHE_CHECK(for struct flock,ac_cv_struct_flock,
    AC_TRY_COMPILE([
#include <unistd.h>
#include <fcntl.h>
        ],
        [struct flock x;],
        [
          ac_cv_struct_flock=yes
        ],[
          ac_cv_struct_flock=no
        ])
)
if test "$ac_cv_struct_flock" = "yes" ; then
    AC_DEFINE(HAVE_STRUCT_FLOCK, 1,[whether you have struct flock])
fi
])

AC_DEFUN(PHP_SOCKLEN_T,[
AC_CACHE_CHECK(for socklen_t,ac_cv_socklen_t,
  AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
socklen_t x;
],[
  ac_cv_socklen_t=yes
],[
  ac_cv_socklen_t=no
]))
if test "$ac_cv_socklen_t" = "yes"; then
  AC_DEFINE(HAVE_SOCKLEN_T, 1, [Whether you have socklen_t])
fi
])

dnl
dnl PHP_SET_SYM_FILE(path)
dnl
dnl set the path of the file which contains the symbol export list
dnl
AC_DEFUN(PHP_SET_SYM_FILE,
[
  PHP_SYM_FILE="$1"
])

dnl
dnl PHP_BUILD_THREAD_SAFE
dnl
AC_DEFUN(PHP_BUILD_THREAD_SAFE,[
  enable_experimental_zts=yes
  if test "$pthreads_working" != "yes"; then
    AC_MSG_ERROR(ZTS currently requires working POSIX threads. We were unable to verify that your system supports Pthreads.)
  fi
])

AC_DEFUN(PHP_REQUIRE_CXX,[
  if test -z "$php_cxx_done"; then
    AC_PROG_CXX
    AC_PROG_CXXCPP
    php_cxx_done=yes
  fi
])

dnl
dnl PHP_BUILD_SHARED
dnl
AC_DEFUN(PHP_BUILD_SHARED,[
  php_build_target=shared
])

dnl
dnl PHP_BUILD_STATIC
dnl
AC_DEFUN(PHP_BUILD_STATIC,[
  php_build_target=static
])

dnl
dnl PHP_BUILD_PROGRAM
dnl
AC_DEFUN(PHP_BUILD_PROGRAM,[
  php_build_target=program
])

dnl
dnl AC_PHP_ONCE(namespace, variable, code)
dnl
dnl execute code, if variable is not set in namespace
dnl
AC_DEFUN(AC_PHP_ONCE,[
  changequote({,})
  unique=`echo $2|sed 's/[^a-zA-Z0-9]/_/g'`
  changequote([,])
  cmd="echo $ac_n \"\$$1$unique$ac_c\""
  if test -n "$unique" && test "`eval $cmd`" = "" ; then
    eval "$1$unique=set"
    $3
  fi
])

dnl
dnl AC_EXPAND_PATH(path, variable)
dnl
dnl expands path to an absolute path and assigns it to variable
dnl
AC_DEFUN(AC_EXPAND_PATH,[
  if test -z "$1" || echo "$1" | grep '^/' >/dev/null ; then
    $2="$1"
  else
    changequote({,})
    ep_dir="`echo $1|sed 's%/*[^/][^/]*/*$%%'`"
    changequote([,])
    ep_realdir="`(cd \"$ep_dir\" && pwd)`"
    $2="$ep_realdir/`basename \"$1\"`"
  fi
])

dnl
dnl AC_ADD_LIBPATH(path[, shared-libadd])
dnl
dnl add a library to linkpath/runpath
dnl
AC_DEFUN(AC_ADD_LIBPATH,[
  if test "$1" != "/usr/lib"; then
    AC_EXPAND_PATH($1, ai_p)
    if test "$ext_shared" = "yes" && test -n "$2"; then
      $2="-R$1 -L$1 [$]$2"
    else
      AC_PHP_ONCE(LIBPATH, $ai_p, [
        test -n "$ld_runpath_switch" && LDFLAGS="$LDFLAGS $ld_runpath_switch$ai_p"
        LDFLAGS="$LDFLAGS -L$ai_p"
        PHP_RPATHS="$PHP_RPATHS $ai_p"
      ])
    fi
  fi
])

dnl
dnl AC_BUILD_RPATH()
dnl
dnl builds RPATH from PHP_RPATHS
dnl
AC_DEFUN(AC_BUILD_RPATH,[
  if test "$PHP_RPATH" = "yes" && test -n "$PHP_RPATHS"; then
    OLD_RPATHS="$PHP_RPATHS"
    PHP_RPATHS=""
    for i in $OLD_RPATHS; do
      PHP_LDFLAGS="$PHP_LDFLAGS -L$i"
      PHP_RPATHS="$PHP_RPATHS -R $i"
      NATIVE_RPATHS="$NATIVE_RPATHS ${ld_runpath_switch}$i"
    done
  fi
])

dnl
dnl AC_ADD_INCLUDE(path)
dnl
dnl add a include path
dnl
AC_DEFUN(AC_ADD_INCLUDE,[
  if test "$1" != "/usr/include"; then
    AC_EXPAND_PATH($1, ai_p)
    AC_PHP_ONCE(INCLUDEPATH, $ai_p, [
      INCLUDES="$INCLUDES -I$ai_p"
    ])
  fi
])

AC_DEFUN(PHP_X_ADD_LIBRARY,[
  ifelse($2,,$3="-l$1 [$]$3", $3="[$]$3 -l$1")
])

dnl
dnl AC_ADD_LIBRARY(library[, append[, shared-libadd]])
dnl
dnl add a library to the link line
dnl
AC_DEFUN(AC_ADD_LIBRARY,[
 case "$1" in
 c|c_r|pthread*) ;;
 *)
ifelse($3,,[
   PHP_X_ADD_LIBRARY($1,$2,LIBS)
],[
   if test "$ext_shared" = "yes"; then
     PHP_X_ADD_LIBRARY($1,$2,$3)
   else
     AC_ADD_LIBRARY($1,$2)
   fi
])
  ;;
  esac
])

dnl
dnl AC_ADD_LIBRARY_DEFER(library[, append])
dnl
dnl add a library to the link line (deferred)
AC_DEFUN(AC_ADD_LIBRARY_DEFER,[
  ifelse($#, 1, DLIBS="-l$1 $DLIBS", DLIBS="$DLIBS -l$1")
])

dnl
dnl AC_ADD_LIBRARY_WITH_PATH(library, path[, shared-libadd])
dnl
dnl add a library to the link line and path to linkpath/runpath.
dnl if shared-libadd is not empty and $ext_shared is yes,
dnl shared-libadd will be assigned the library information
dnl
AC_DEFUN(AC_ADD_LIBRARY_WITH_PATH,[
ifelse($3,,[
  if test -n "$2"; then
    AC_ADD_LIBPATH($2)
  fi
  AC_ADD_LIBRARY($1)
],[
  if test "$ext_shared" = "yes"; then
    $3="-l$1 [$]$3"
    if test -n "$2"; then
      AC_ADD_LIBPATH($2,$3)
    fi
  else
    AC_ADD_LIBRARY_WITH_PATH($1,$2)
  fi
])
])

dnl
dnl AC_ADD_LIBRARY_DEFER_WITH_PATH(library, path)
dnl
dnl add a library to the link line (deferred)
dnl and path to linkpath/runpath (not deferred)
dnl
AC_DEFUN(AC_ADD_LIBRARY_DEFER_WITH_PATH,[
  AC_ADD_LIBPATH($2)
  AC_ADD_LIBRARY_DEFER($1)
])

AC_DEFUN(AM_SET_LIBTOOL_VARIABLE,[
  LIBTOOL='$(SHELL) $(top_builddir)/libtool $1'
])

dnl
dnl Check for cc option
dnl
AC_DEFUN(AC_CHECK_CC_OPTION,[
  echo "main(){return 0;}" > conftest.$ac_ext
  opt="$1"
  changequote({,})
  var=`echo $opt|sed 's/[^a-zA-Z0-9]/_/g'`
  changequote([,])
  AC_MSG_CHECKING([if compiler supports -$1 really])
  ac_php_compile="${CC-cc} -$opt -o conftest $CFLAGS $CPPFLAGS conftest.$ac_ext 2>&1"
  if eval $ac_php_compile 2>&1 | egrep "$opt" > /dev/null 2>&1 ; then
    eval php_cc_$var=no
	AC_MSG_RESULT(no)
  else
    if eval ./conftest 2>/dev/null ; then
      eval php_cc_$var=yes
	  AC_MSG_RESULT(yes)
    else
      eval php_cc_$var=no
	  AC_MSG_RESULT(no)
    fi
  fi
])

AC_DEFUN(PHP_REGEX,[

if test "$REGEX_TYPE" = "php"; then
  REGEX_LIB=regex/libregex.la
  REGEX_DIR=regex
  AC_DEFINE(HSREGEX,1,[ ])
  AC_DEFINE(REGEX,1,[ ])
  PHP_FAST_OUTPUT(regex/Makefile)
elif test "$REGEX_TYPE" = "system"; then
  AC_DEFINE(REGEX,0,[ ])
fi

AC_MSG_CHECKING(which regex library to use)
AC_MSG_RESULT($REGEX_TYPE)

PHP_SUBST(REGEX_DIR)
PHP_SUBST(REGEX_LIB)
PHP_SUBST(HSREGEX)
])

dnl
dnl See if we have broken header files like SunOS has.
dnl
AC_DEFUN(AC_MISSING_FCLOSE_DECL,[
  AC_MSG_CHECKING([for fclose declaration])
  AC_TRY_COMPILE([#include <stdio.h>],[int (*func)() = fclose],[
    AC_DEFINE(MISSING_FCLOSE_DECL,0,[ ])
    AC_MSG_RESULT(ok)
  ],[
    AC_DEFINE(MISSING_FCLOSE_DECL,1,[ ])
    AC_MSG_RESULT(missing)
  ])
])

dnl
dnl Check for broken sprintf()
dnl
AC_DEFUN(AC_BROKEN_SPRINTF,[
  AC_CACHE_CHECK(whether sprintf is broken, ac_cv_broken_sprintf,[
    AC_TRY_RUN([main() {char buf[20];exit(sprintf(buf,"testing 123")!=11); }],[
      ac_cv_broken_sprintf=no
    ],[
      ac_cv_broken_sprintf=yes
    ],[
      ac_cv_broken_sprintf=no
    ])
  ])
  if test "$ac_cv_broken_sprintf" = "yes"; then
    AC_DEFINE(PHP_BROKEN_SPRINTF, 1, [ ])
  else
    AC_DEFINE(PHP_BROKEN_SPRINTF, 0, [ ])
  fi
])

dnl
dnl PHP_EXTENSION(extname [, shared])
dnl
dnl Includes an extension in the build.
dnl
dnl "extname" is the name of the ext/ subdir where the extension resides
dnl "shared" can be set to "shared" or "yes" to build the extension as
dnl a dynamically loadable library.
dnl
AC_DEFUN(PHP_EXTENSION,[
  EXT_SUBDIRS="$EXT_SUBDIRS $1"
  
  if test -d "$abs_srcdir/ext/$1"; then
dnl ---------------------------------------------- Internal Module
    ext_builddir="ext/$1"
    ext_srcdir="$abs_srcdir/ext/$1"
  else
dnl ---------------------------------------------- External Module
    ext_builddir="."
    ext_srcdir="$abs_srcdir"
  fi

  if test "$2" != "shared" && test "$2" != "yes"; then
dnl ---------------------------------------------- Static module
    LIB_BUILD($ext_builddir)
    EXT_LTLIBS="$EXT_LTLIBS $ext_builddir/lib$1.la"
    EXT_STATIC="$EXT_STATIC $1"
  else 
dnl ---------------------------------------------- Shared module
    LIB_BUILD($ext_builddir,yes)
    AC_DEFINE_UNQUOTED([COMPILE_DL_]translit($1,a-z-,A-Z_), 1, Whether to build $1 as dynamic module)
  fi

  PHP_FAST_OUTPUT($ext_builddir/Makefile)
])

PHP_SUBST(EXT_SUBDIRS)
PHP_SUBST(EXT_STATIC)
PHP_SUBST(EXT_SHARED)
PHP_SUBST(EXT_LIBS)
PHP_SUBST(EXT_LTLIBS)

dnl
dnl Solaris requires main code to be position independent in order
dnl to let shared objects find symbols.  Weird.  Ugly.
dnl
dnl Must be run after all --with-NN options that let the user
dnl choose dynamic extensions, and after the gcc test.
dnl
AC_DEFUN(PHP_SOLARIS_PIC_WEIRDNESS,[
  AC_MSG_CHECKING(whether -fPIC is required)
  if test "$EXT_SHARED" != ""; then
    os=`uname -sr 2>/dev/null`
    case "$os" in
        "SunOS 5.6"|"SunOS 5.7")
          case "$CC" in
	    gcc*|egcs*) CFLAGS="$CFLAGS -fPIC";;
	    *) CFLAGS="$CFLAGS -fpic";;
	  esac
	  AC_MSG_RESULT(yes);;
	*)
	  AC_MSG_RESULT(no);;
    esac
  else
    AC_MSG_RESULT(no)
  fi
])

dnl
dnl Checks whether $withval is "shared" or starts with "shared,XXX"
dnl and sets $shared to "yes" or "no", and removes "shared,?" stuff
dnl from $withval.
dnl
AC_DEFUN(PHP_WITH_SHARED,[
    case $withval in
	shared)
	    shared=yes
	    withval=yes
	    ;;
	shared,*)
	    shared=yes
	    withval=`echo $withval | sed -e 's/^shared,//'`      
	    ;;
	*)
	    shared=no
	    ;;
    esac
    if test -n "$php_always_shared"; then
		shared=yes
	fi
])

dnl The problem is that the default compilation flags in Solaris 2.6 won't
dnl let programs access large files;  you need to tell the compiler that
dnl you actually want your programs to work on large files.  For more
dnl details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html

dnl Written by Paul Eggert <eggert@twinsun.com>.

AC_DEFUN(AC_SYS_LFS,
[dnl
  # If available, prefer support for large files unless the user specified
  # one of the CPPFLAGS, LDFLAGS, or LIBS variables.
  AC_MSG_CHECKING(whether large file support needs explicit enabling)
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
    eval ac_test_$ac_shellvar=\$ac_getconf
  done
  case "$ac_result$ac_getconfs" in
    yes) ac_result=no ;;
  esac
  case "$ac_result$ac_set" in
    yes?*) ac_result="yes, but $ac_set is already set, so use its settings"
  esac
  AC_MSG_RESULT($ac_result)
  case $ac_result in
    yes)
      for ac_shellvar in $ac_shellvars; do
        eval $ac_shellvar=\$ac_test_$ac_shellvar
      done ;;
  esac
])

AC_DEFUN(AC_SOCKADDR_SA_LEN,[
  AC_CACHE_CHECK([for field sa_len in struct sockaddr],ac_cv_sockaddr_sa_len,[
    AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
    [struct sockaddr s; s.sa_len;],
    [ac_cv_sockaddr_sa_len=yes
     AC_DEFINE(HAVE_SOCKADDR_SA_LEN,1,[ ])],
    [ac_cv_sockaddr_sa_len=no])
  ])
])


dnl ## PHP_OUTPUT(file)
dnl ## adds "file" to the list of files generated by AC_OUTPUT
dnl ## This macro can be used several times.
AC_DEFUN(PHP_OUTPUT,[
  PHP_OUTPUT_FILES="$PHP_OUTPUT_FILES $1"
])

AC_DEFUN(PHP_DECLARED_TIMEZONE,[
  AC_CACHE_CHECK(for declared timezone, ac_cv_declared_timezone,[
    AC_TRY_COMPILE([
#include <sys/types.h>
#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
],[
    time_t foo = (time_t) timezone;
],[
  ac_cv_declared_timezone=yes
],[
  ac_cv_declared_timezone=no
])])
  if test "$ac_cv_declared_timezone" = "yes"; then
    AC_DEFINE(HAVE_DECLARED_TIMEZONE, 1, [Whether system headers declare timezone])
  fi
])

AC_DEFUN(PHP_EBCDIC,[
  AC_CACHE_CHECK([whether system uses EBCDIC],ac_cv_ebcdic,[
  AC_TRY_RUN( [
int main(void) { 
  return (unsigned char)'A' != (unsigned char)0xC1; 
} 
],[
  ac_cv_ebcdic="yes"
],[
  ac_cv_ebcdic="no"
],[
  ac_cv_ebcdic="no"
])])
  if test "$ac_cv_ebcdic" = "yes"; then
    AC_DEFINE(CHARSET_EBCDIC,1, [Define if system uses EBCDIC])
  fi
])

AC_DEFUN(PHP_FOPENCOOKIE,[
	AC_CHECK_FUNC(fopencookie, [ have_glibc_fopencookie=yes ])

	if test "$have_glibc_fopencookie" = "yes" ; then
	  	dnl this comes in two flavors:
      dnl newer glibcs (since 2.1.2 ? )
      dnl have a type called cookie_io_functions_t
		  AC_TRY_COMPILE([ #define _GNU_SOURCE
                       #include <stdio.h>
									   ],
	                   [ cookie_io_functions_t cookie; ],
                     [ have_cookie_io_functions_t=yes ],
										 [ ] )

		  if test "$have_cookie_io_functions_t" = "yes" ; then
        cookie_io_functions_t=cookie_io_functions_t
	      have_fopen_cookie=yes
      else
  	    dnl older glibc versions (up to 2.1.2 ?)
        dnl call it _IO_cookie_io_functions_t
		    AC_TRY_COMPILE([ #define _GNU_SOURCE
                       #include <stdio.h>
									   ],
	                   [ _IO_cookie_io_functions_t cookie; ],
                     [ have_IO_cookie_io_functions_t=yes ],
										 [] )
		    if test "$have_cookie_io_functions_t" = "yes" ; then
          cookie_io_functions_t=_IO_cookie_io_functions_t
	        have_fopen_cookie=yes
		    fi
			fi

		  if test "$have_fopen_cookie" = "yes" ; then
		    AC_DEFINE(HAVE_FOPENCOOKIE, 1, [ ])
			  AC_DEFINE_UNQUOTED(COOKIE_IO_FUNCTIONS_T, $cookie_io_functions_t, [ ])
      fi      

  	fi
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

# Like AC_CONFIG_HEADER, but automatically create stamp file.

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

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


# serial 1

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it handles function prototypes correctly.
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN(AM_PROG_CC_STDC,
[AC_REQUIRE([AC_PROG_CC])
AC_BEFORE([$0], [AC_C_INLINE])
AC_BEFORE([$0], [AC_C_CONST])
dnl Force this before AC_PROG_CPP.  Some cpp's, eg on HPUX, require
dnl a magic option to avoid problems with ANSI preprocessor commands
dnl like #elif.
dnl FIXME: can't do this because then AC_AIX won't work due to a
dnl circular dependency.
dnl AC_BEFORE([$0], [AC_PROG_CPP])
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" "-Xc -D__EXTENSIONS__"
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
/* Most of the following tests are stolen from RCS 5.7's src/conf.sh.  */
struct buf { int x; };
FILE * (*rcsopen) (struct buf *, struct stat *, int);
static char *e (p, i)
     char **p;
     int i;
{
  return p[i];
}
static char *f (char * (*g) (char **, int), char **p, ...)
{
  char *s;
  va_list v;
  va_start (v,p);
  s = g (p, va_arg (v,int));
  va_end (v);
  return s;
}
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};
int pairnames (int, char **, FILE *(*)(struct buf *, struct stat *, int), int, int);
int argc;
char **argv;
], [
return f (e, argv, 0) != argv[0]  ||  f (e, argv, 1) != argv[1];
],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
if test -z "$am_cv_prog_cc_stdc"; then
  AC_MSG_RESULT([none needed])
else
  AC_MSG_RESULT($am_cv_prog_cc_stdc)
fi
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])


dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, "$missing_dir/missing flex")
AC_PROG_LEX
AC_DECL_YYTEXT])


# serial 40 AC_PROG_LIBTOOL
AC_DEFUN(AC_PROG_LIBTOOL,
[AC_REQUIRE([AC_LIBTOOL_SETUP])dnl

# Save cache, so that ltconfig can load it
AC_CACHE_SAVE

# Actually configure libtool.  ac_aux_dir is where install-sh is found.
CC="$CC" CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" \
LD="$LD" LDFLAGS="$LDFLAGS" LIBS="$LIBS" \
LN_S="$LN_S" NM="$NM" RANLIB="$RANLIB" \
DLLTOOL="$DLLTOOL" AS="$AS" OBJDUMP="$OBJDUMP" \
${CONFIG_SHELL-/bin/sh} $ac_aux_dir/ltconfig --no-reexec \
$libtool_flags --no-verify $ac_aux_dir/ltmain.sh $lt_target \
|| AC_MSG_ERROR([libtool configure failed])

# Reload cache, that may have been modified by ltconfig
AC_CACHE_LOAD

# This can be used to rebuild libtool when needed
LIBTOOL_DEPS="$ac_aux_dir/ltconfig $ac_aux_dir/ltmain.sh"

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

# Redirect the config.log output again, so that the ltconfig log is not
# clobbered by the next message.
exec 5>>./config.log
])

AC_DEFUN(AC_LIBTOOL_SETUP,
[AC_PREREQ(2.13)dnl
AC_REQUIRE([AC_ENABLE_SHARED])dnl
AC_REQUIRE([AC_ENABLE_STATIC])dnl
AC_REQUIRE([AC_ENABLE_FAST_INSTALL])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_REQUIRE([AC_PROG_RANLIB])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_LD])dnl
AC_REQUIRE([AC_PROG_NM])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
dnl

case "$target" in
NONE) lt_target="$host" ;;
*) lt_target="$target" ;;
esac

# Check for any special flags to pass to ltconfig.
libtool_flags="--cache-file=$cache_file"
test "$enable_shared" = no && libtool_flags="$libtool_flags --disable-shared"
test "$enable_static" = no && libtool_flags="$libtool_flags --disable-static"
test "$enable_fast_install" = no && libtool_flags="$libtool_flags --disable-fast-install"
test "$ac_cv_prog_gcc" = yes && libtool_flags="$libtool_flags --with-gcc"
test "$ac_cv_prog_gnu_ld" = yes && libtool_flags="$libtool_flags --with-gnu-ld"
ifdef([AC_PROVIDE_AC_LIBTOOL_DLOPEN],
[libtool_flags="$libtool_flags --enable-dlopen"])
ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[libtool_flags="$libtool_flags --enable-win32-dll"])
AC_ARG_ENABLE(libtool-lock,
  [  --disable-libtool-lock  avoid locking (might break parallel builds)])
test "x$enable_libtool_lock" = xno && libtool_flags="$libtool_flags --disable-lock"
test x"$silent" = xyes && libtool_flags="$libtool_flags --silent"

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case "$lt_target" in
*-*-irix6*)
  # Find out which ABI we are using.
  echo '[#]line __oline__ "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case "`/usr/bin/file conftest.o`" in
    *32-bit*)
      LD="${LD-ld} -32"
      ;;
    *N32*)
      LD="${LD-ld} -n32"
      ;;
    *64-bit*)
      LD="${LD-ld} -64"
      ;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -belf"
  AC_CACHE_CHECK([whether the C compiler needs -belf], lt_cv_cc_needs_belf,
    [AC_TRY_LINK([],[],[lt_cv_cc_needs_belf=yes],[lt_cv_cc_needs_belf=no])])
  if test x"$lt_cv_cc_needs_belf" != x"yes"; then
    # this is probably gcc 2.8.0, egcs 1.0 or newer; no need for -belf
    CFLAGS="$SAVE_CFLAGS"
  fi
  ;;

ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[*-*-cygwin* | *-*-mingw*)
  AC_CHECK_TOOL(DLLTOOL, dlltool, false)
  AC_CHECK_TOOL(AS, as, false)
  AC_CHECK_TOOL(OBJDUMP, objdump, false)
  ;;
])
esac
])

# AC_LIBTOOL_DLOPEN - enable checks for dlopen support
AC_DEFUN(AC_LIBTOOL_DLOPEN, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])])

# AC_LIBTOOL_WIN32_DLL - declare package support for building win32 dll's
AC_DEFUN(AC_LIBTOOL_WIN32_DLL, [AC_BEFORE([$0], [AC_LIBTOOL_SETUP])])

# AC_ENABLE_SHARED - implement the --enable-shared flag
# Usage: AC_ENABLE_SHARED[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_SHARED, [dnl
define([AC_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]  build shared libraries [default=>>AC_ENABLE_SHARED_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_shared=yes ;;
no) enable_shared=no ;;
*)
  enable_shared=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_shared=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_shared=AC_ENABLE_SHARED_DEFAULT)dnl
])

# AC_DISABLE_SHARED - set the default shared flag to --disable-shared
AC_DEFUN(AC_DISABLE_SHARED, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_SHARED(no)])

# AC_ENABLE_STATIC - implement the --enable-static flag
# Usage: AC_ENABLE_STATIC[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_STATIC, [dnl
define([AC_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]  build static libraries [default=>>AC_ENABLE_STATIC_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_static=yes ;;
no) enable_static=no ;;
*)
  enable_static=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_static=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_static=AC_ENABLE_STATIC_DEFAULT)dnl
])

# AC_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN(AC_DISABLE_STATIC, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_STATIC(no)])


# AC_ENABLE_FAST_INSTALL - implement the --enable-fast-install flag
# Usage: AC_ENABLE_FAST_INSTALL[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_FAST_INSTALL, [dnl
define([AC_ENABLE_FAST_INSTALL_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(fast-install,
changequote(<<, >>)dnl
<<  --enable-fast-install[=PKGS]  optimize for fast installation [default=>>AC_ENABLE_FAST_INSTALL_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_fast_install=yes ;;
no) enable_fast_install=no ;;
*)
  enable_fast_install=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_fast_install=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_fast_install=AC_ENABLE_FAST_INSTALL_DEFAULT)dnl
])

# AC_ENABLE_FAST_INSTALL - set the default to --disable-fast-install
AC_DEFUN(AC_DISABLE_FAST_INSTALL, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_FAST_INSTALL(no)])

# AC_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN(AC_PROG_LD,
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
ac_prog=ld
if test "$ac_cv_prog_gcc" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  ac_prog=`($CC -print-prog-name=ld) 2>&5`
  case "$ac_prog" in
    # Accept absolute paths.
changequote(,)dnl
    [\\/]* | [A-Za-z]:[\\/]*)
      re_direlt='/[^/][^/]*/\.\./'
changequote([,])dnl
      # Canonicalize the path of ld
      ac_prog=`echo $ac_prog| sed 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(ac_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      ac_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$ac_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
	test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  ac_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$ac_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_PROG_LD_GNU
])

AC_DEFUN(AC_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

# AC_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN(AC_PROG_NM,
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(ac_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  ac_cv_path_NM="$NM"
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH /usr/ccs/bin /usr/ucb /bin; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/nm || test -f $ac_dir/nm$ac_exeext ; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1q' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      if ($ac_dir/nm -B /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -B"
	break
      elif ($ac_dir/nm -p /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -p"
	break
      else
	ac_cv_path_NM=${ac_cv_path_NM="$ac_dir/nm"} # keep the first match, but
	continue # so that we can try to find one that supports BSD flags
      fi
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$ac_cv_path_NM" && ac_cv_path_NM=nm
fi])
NM="$ac_cv_path_NM"
AC_MSG_RESULT([$NM])
])

# AC_CHECK_LIBM - check for math library
AC_DEFUN(AC_CHECK_LIBM,
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case "$lt_target" in
*-*-beos* | *-*-cygwin*)
  # These system don't have libm
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM="-lmw")
  AC_CHECK_LIB(m, main, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, main, LIBM="-lm")
  ;;
esac
])

# AC_LIBLTDL_CONVENIENCE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl convenience library and INCLTDL to the include flags for
# the libltdl header and adds --enable-ltdl-convenience to the
# configure arguments.  Note that LIBLTDL and INCLTDL are not
# AC_SUBSTed, nor is AC_CONFIG_SUBDIRS called.  If DIR is not
# provided, it is assumed to be `libltdl'.  LIBLTDL will be prefixed
# with '${top_builddir}/' and INCLTDL will be prefixed with
# '${top_srcdir}/' (note the single quotes!).  If your package is not
# flat and you're not using automake, define top_builddir and
# top_srcdir appropriately in the Makefiles.
AC_DEFUN(AC_LIBLTDL_CONVENIENCE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  case "$enable_ltdl_convenience" in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
  esac
  LIBLTDL='${top_builddir}/'ifelse($#,1,[$1],['libltdl'])/libltdlc.la
  INCLTDL='-I${top_srcdir}/'ifelse($#,1,[$1],['libltdl'])
])

# AC_LIBLTDL_INSTALLABLE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl installable library and INCLTDL to the include flags for
# the libltdl header and adds --enable-ltdl-install to the configure
# arguments.  Note that LIBLTDL and INCLTDL are not AC_SUBSTed, nor is
# AC_CONFIG_SUBDIRS called.  If DIR is not provided and an installed
# libltdl is not found, it is assumed to be `libltdl'.  LIBLTDL will
# be prefixed with '${top_builddir}/' and INCLTDL will be prefixed
# with '${top_srcdir}/' (note the single quotes!).  If your package is
# not flat and you're not using automake, define top_builddir and
# top_srcdir appropriately in the Makefiles.
# In the future, this macro may have to be called after AC_PROG_LIBTOOL.
AC_DEFUN(AC_LIBLTDL_INSTALLABLE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  AC_CHECK_LIB(ltdl, main,
  [test x"$enable_ltdl_install" != xyes && enable_ltdl_install=no],
  [if test x"$enable_ltdl_install" = xno; then
     AC_MSG_WARN([libltdl not installed, but installation disabled])
   else
     enable_ltdl_install=yes
   fi
  ])
  if test x"$enable_ltdl_install" = x"yes"; then
    ac_configure_args="$ac_configure_args --enable-ltdl-install"
    LIBLTDL='${top_builddir}/'ifelse($#,1,[$1],['libltdl'])/libltdl.la
    INCLTDL='-I${top_srcdir}/'ifelse($#,1,[$1],['libltdl'])
  else
    ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
    LIBLTDL="-lltdl"
    INCLTDL=
  fi
])

dnl old names
AC_DEFUN(AM_PROG_LIBTOOL, [indir([AC_PROG_LIBTOOL])])dnl
AC_DEFUN(AM_ENABLE_SHARED, [indir([AC_ENABLE_SHARED], $@)])dnl
AC_DEFUN(AM_ENABLE_STATIC, [indir([AC_ENABLE_STATIC], $@)])dnl
AC_DEFUN(AM_DISABLE_SHARED, [indir([AC_DISABLE_SHARED], $@)])dnl
AC_DEFUN(AM_DISABLE_STATIC, [indir([AC_DISABLE_STATIC], $@)])dnl
AC_DEFUN(AM_PROG_LD, [indir([AC_PROG_LD])])dnl
AC_DEFUN(AM_PROG_NM, [indir([AC_PROG_NM])])dnl

dnl This is just to silence aclocal about the macro not being used
ifelse([AC_DISABLE_FAST_INSTALL])dnl

