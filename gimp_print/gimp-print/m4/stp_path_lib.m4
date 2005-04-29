dnl @synopsis STP_PATH_LIB(LIBRARY [, MINIMUM-VERSION [, HEADERS [, CONFIG-SCRIPT [, MODULES [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]]]]])
dnl
dnl Runs a LIBRARY-config script and defines LIBRARY_CFLAGS and LIBRARY_LIBS,
dnl saving you from writing your own macro specific to your library.
dnl
dnl The options:
dnl  $1 = LIBRARY             e.g. gtk, ncurses, z, gimpprint
dnl  $2 = MINIMUM-VERSION     x.y.z format e.g. 4.2.1
dnl                           Add ' -nocheck' e.g. '4.2.1 -nocheck' to avoid
dnl                           checking version with library-defined version
dnl                           numbers (see below) i.e. --version only
dnl  $3 = CONFIG-SCRIPT       Name of libconfig script if not
dnl                           LIBRARY-config
dnl  $4 = MODULES             List of module names to pass to LIBRARY-config.
dnl                           It is probably best to use only one, to avoid
dnl                           two version numbers being reported.
dnl  $5 = ACTION-IF-FOUND     Shell script to run if library is found
dnl  $6 = ACTION-IF-NOT-FOUND Shell script to run if library is not found
dnl
dnl pkg-config will be used to obtain cflags, libs and version data by
dnl default.  You must have a library.pc file installed.  Two macros
dnl enable and disable pkgconfig support:
dnl   STP_PATH_LIB_PKGCONFIG   Enable pkg-config support
dnl   STP_PATH_LIB_LIBCONFIG   Disable pkg-config support
dnl When pkg-config is enabled, CONFIG_SCRIPT will be ignored.
dnl
dnl LIBRARY-config must support `--cflags' and `--libs' args.
dnl If MINIMUM-VERSION is specified, LIBRARY-config should also support the
dnl `--version' arg, and have version information embedded in its header
dnl as detailed below:
dnl
dnl Macros:
dnl  #define LIBRARY_MAJOR_VERSION       (@LIBRARY_MAJOR_VERSION@)
dnl  #define LIBRARY_MINOR_VERSION       (@LIBRARY_MINOR_VERSION@)
dnl  #define LIBRARY_MICRO_VERSION       (@LIBRARY_MICRO_VERSION@)
dnl
dnl Version numbers (defined in the library):
dnl  extern const unsigned int library_major_version;
dnl  extern const unsigned int library_minor_version;
dnl  extern const unsigned int library_micro_version;
dnl
dnl If a different naming scheme is used, this may be specified with
dnl STP_PATH_LIB_REGISTER (see below). For example:
dnl   STP_PATH_LIB_REGISTER([_STP_PATH_LIB_VERSION_PREFIX], [mylib])
dnl
dnl If the header to include is not called LIBRARY/LIBRARY.h, an
dnl alternate header may be specified with STP_PATH_LIB_REGISTER.  All
dnl changes are reset to the defaults when the macro completes.
dnl
dnl If the above are not defined, then use ' -nocheck'.
dnl
dnl If the `--with-library-[exec-]prefix' arguments to ./configure are
dnl given, it must also support `--prefix' and `--exec-prefix'.
dnl (In other words, it must be like gtk-config.)
dnl
dnl If modules are to be used, LIBRARY-config must support modules.
dnl
dnl For example:
dnl  STP_PATH_LIB(foo, 1.0.0)
dnl would run `foo-config --version' and check that it is at least 1.0.0
dnl
dnl If so, the following would then be defined:
dnl   FOO_CFLAGS  to `foo-config --cflags`
dnl   FOO_LIBS    to `foo-config --libs`
dnl   FOO_VERSION to `foo-config --version`
dnl
dnl Based on `AM_PATH_GTK' (gtk.m4) by Owen Taylor, and `AC_PATH_GENERIC'
dnl (ac_path_generic.m4) by Angus Lees <gusl@cse.unsw.edu.au>.
dnl pkg-config support based on AM_PATH_GTK_2_0 (gtk-2.0.m4) by Owen Taylor.
dnl
dnl @version $Id: stp_path_lib.m4,v 1.1.1.1 2004/07/23 06:26:28 jlovell Exp $
dnl @author Roger Leigh <rleigh@debian.org>
dnl


## Table of Contents:
## 1. The main macro
## 2. Core macros
## 3. Integrity checks
## 4. Error reporting
## 5. Feature macros


## ------------------ ##
## 1. The main macro. ##
## ------------------ ##


# STP_PATH_LIB(LIBRARY, MINIMUM-VERSION, CONFIG-SCRIPT,
#              MODULES, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
# -----------------------------------------------------------
# Check for the presence of libLIBRARY, with a minumum version
# MINIMUM-VERSION.
# 
# If needed, use the libconfig script CONFIG-SCRIPT.  If the script
# needs extra modules specifying, pass them as MODULES.
# 
# Run ACTION-IF-FOUND if the library is present and all tests pass, or
# ACTION-IF-NOT-FOUND if it is not present or any tests fail.
AC_DEFUN([STP_PATH_LIB],[# check for presence of lib$1
dnl We're going to need uppercase, lowercase and user-friendly
dnl versions of the string `LIBRARY', and long and cache variants.
m4_pushdef([UP], m4_translit([$1], [a-z], [A-Z]))dnl
m4_pushdef([DOWN], m4_translit([$1], [A-Z], [a-z]))dnl
m4_pushdef([LDOWN], ac_path_lib_[]DOWN)dnl
m4_pushdef([CACHE], ac_cv_path_lib_[]DOWN)dnl
m4_pushdef([ERRORLOG], error.[]DOWN[]test)dnl
_STP_PATH_LIB_INIT([$1], [$3])
_STP_PATH_LIB_CHECK([$1], [$2], [$4])
_STP_PATH_LIB_CHECK_TESTS([$2])
_STP_PATH_LIB_ERROR_DUMP
_STP_PATH_LIB_FINI([$5], [$6])
dnl Pop the macros we defined earlier.
m4_popdef([UP])dnl
m4_popdef([DOWN])dnl
m4_popdef([LDOWN])dnl
m4_popdef([CACHE])dnl
m4_popdef([ERRORLOG])dnl
])# STP_PATH_LIB




## --------------- ##
## 2. Core macros. ##
## --------------- ##


# _STP_PATH_LIB_INIT(LIBRARY, CONFIG-SCRIPT)
# ------------------------------------------
# Initialisation of defaults and options.
# Remove error log from previous runs.
AC_DEFUN([_STP_PATH_LIB_INIT],
[_STP_PATH_LIB_DEFAULTS([$1], [$2])
_STP_PATH_LIB_OPTIONS
rm -f ERRORLOG
# Save variables in case check fails.
ac_save_[]UP[]_CFLAGS="$UP[]_CFLAGS"
ac_save_[]UP[]_LIBS="$UP[]_LIBS"
ac_save_[]UP[]_VERSION="$UP[]_VERSION"
])


# _STP_PATH_LIB_DEFAULTS(LIBRARY, HEADER, CONFIG-SCRIPT)
# ------------------------------------------------------
# Set up default behaviour.
AC_DEFUN([_STP_PATH_LIB_DEFAULTS],
[dnl Set up pkgconfig as default config script.
m4_ifdef([STP_PATH_LIB_USEPKGCONFIG],, [STP_PATH_LIB_PKGCONFIG])
dnl Set default header and config script names.
LDOWN[]_header="m4_default([_STP_PATH_LIB_HEADER], [$1/$1.h])"
LDOWN[]_config="m4_default([$2], [$1-config])"
ifdef([_STP_PATH_LIB_VERSION_PREFIX],,
      [m4_define([_STP_PATH_LIB_VERSION_PREFIX],
                 DOWN[_])
      ])
ifdef([_STP_PATH_LIB_VERSION_MAJOR],,
      [m4_define([_STP_PATH_LIB_VERSION_MAJOR],
                 [major])
      ])
ifdef([_STP_PATH_LIB_VERSION_MINOR],,
      [m4_define([_STP_PATH_LIB_VERSION_MINOR],
                 [minor])
      ])
ifdef([_STP_PATH_LIB_VERSION_MICRO],,
      [m4_define([_STP_PATH_LIB_VERSION_MICRO],
                 [micro])
      ])
ifdef([_STP_PATH_LIB_VERSION_SUFFIX],,
      [m4_define([_STP_PATH_LIB_VERSION_SUFFIX],
                 [_version])
      ])
ifdef([_STP_PATH_LIB_DEFVERSION_PREFIX],,
      [m4_define([_STP_PATH_LIB_DEFVERSION_PREFIX],
                 UP[_])
      ])
ifdef([_STP_PATH_LIB_DEFVERSION_MAJOR],,
      [m4_define([_STP_PATH_LIB_DEFVERSION_MAJOR],
                 [MAJOR])
      ])
ifdef([_STP_PATH_LIB_DEFVERSION_MINOR],,
      [m4_define([_STP_PATH_LIB_DEFVERSION_MINOR],
                 [MINOR])
      ])
ifdef([_STP_PATH_LIB_DEFVERSION_MICRO],,
      [m4_define([_STP_PATH_LIB_DEFVERSION_MICRO],
                 [MICRO])
      ])
ifdef([_STP_PATH_LIB_DEFVERSION_SUFFIX],,
      [m4_define([_STP_PATH_LIB_DEFVERSION_SUFFIX],
                 [_VERSION])
      ])
])# _STP_PATH_LIB_DEFAULTS


# _STP_PATH_LIB_OPTIONS
# ---------------------
# configure options.
AC_DEFUN([_STP_PATH_LIB_OPTIONS],
[m4_if(STP_PATH_LIB_USEPKGCONFIG, [true], ,
[AC_ARG_WITH(DOWN-prefix,
            AC_HELP_STRING([--with-DOWN-prefix=PFX],
	                   [prefix where UP is installed (optional)]),
            [LDOWN[]_config_prefix="$withval"],
	    [LDOWN[]_config_prefix=""])dnl
AC_ARG_WITH(DOWN-exec-prefix,
            AC_HELP_STRING([--with-DOWN-exec-prefix=PFX],
	                   [exec-prefix where UP is installed (optional)]),
            [LDOWN[]_config_exec_prefix="$withval"],
	    [LDOWN[]_config_exec_prefix=""])dnl
])
AC_ARG_ENABLE(DOWN[]test,
              AC_HELP_STRING([--disable-DOWN[]test],
	                     [do not try to compile and run a test UP program]),
              [LDOWN[]_test_enabled="no"],
              [LDOWN[]_test_enabled="yes"])dnl
])# _STP_PATH_LIB_OPTIONS


# _STP_PATH_LIB_CHECK(LIBRARY, MINIMUM-VERSION, MODULES)
# ------------------------------------------------------
# Obtain library CFLAGS, LIBS and VERSION information.  Cache results,
# but set avoidcache to no if config program is not available.  Break
# up available and minumum version into major, minor and micro version
# numbers.
AC_DEFUN([_STP_PATH_LIB_CHECK],
[
# Set up LIBRARY-config script parameters
m4_if([$3], , ,
      [LDOWN[]_config_args="$LDOWN[]_config_args $3"])
LDOWN[]_min_version=`echo "$2" | sed -e 's/ -nocheck//'`
m4_if([$2], , ,[if test "$LDOWN[]_min_version" = "$2" ; then
                  LDOWN[]_version_test_enabled="yes"
                fi])
m4_if(STP_PATH_LIB_USEPKGCONFIG, [true],
[LDOWN[]_config_args="$1 $LDOWN[]_config_args"
],
[  if test x$LDOWN[]_config_exec_prefix != x ; then
    LDOWN[]_config_args="$LDOWN[]_config_args --exec-prefix=$LDOWN[]_config_exec_prefix"
  fi
  if test x$LDOWN[]_config_prefix != x ; then
    LDOWN[]_config_args="$LDOWN[]_config_args --prefix=$LDOWN[]_config_prefix"
  fi
])
dnl set --version for libconfig or --modversion for pkgconfig
m4_if(STP_PATH_LIB_USEPKGCONFIG, [true],
      [AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
       if test x$PKG_CONFIG != xno ; then
         if pkg-config --atleast-pkgconfig-version 0.7 ; then
           :
         else
           STP_PATH_LIB_ERROR([A new enough version of pkg-config was not found:])
           STP_PATH_LIB_ERROR([version 0.7 or better required.])
           STP_PATH_LIB_ERROR([See http://pkgconfig.sourceforge.net])
           PKG_CONFIG=no
         fi
       fi
       UP[]_CONFIG="$PKG_CONFIG"
       LDOWN[]_config="pkg-config"
       m4_pushdef([LIBCONFIG_CFLAGS], [--cflags])
       m4_pushdef([LIBCONFIG_LIBS], [--libs])
       m4_pushdef([LIBCONFIG_VERSION], [--modversion])
      ],
      [AC_PATH_PROG(UP[]_CONFIG, $LDOWN[]_config, no)
       m4_pushdef([LIBCONFIG_CFLAGS], [--cflags])
       m4_pushdef([LIBCONFIG_LIBS], [--libs])
       m4_pushdef([LIBCONFIG_VERSION], [--version])
       if test x$UP[]_CONFIG = xno ; then
         STP_PATH_LIB_ERROR([The $LDOWN[]_config script installed by UP could not be found.])
         STP_PATH_LIB_ERROR([If UP was installed in PREFIX, make sure PREFIX/bin is in])
         STP_PATH_LIB_ERROR([your path, or set the UP[]_CONFIG environment variable to the])
         STP_PATH_LIB_ERROR([full path to $LDOWN[]_config.])
       fi
      ])

if test x$UP[]_CONFIG = xno ; then
  LDOWN[]_present_avoidcache="no"
else
  LDOWN[]_present_avoidcache="yes"

  AC_CACHE_CHECK([for UP CFLAGS],
                 [CACHE[]_cflags],
                 [CACHE[]_cflags=`$UP[]_CONFIG $LDOWN[]_config_args LIBCONFIG_CFLAGS`])
  AC_CACHE_CHECK([for UP LIBS],
                 [CACHE[]_libs],
                 [CACHE[]_libs=`$UP[]_CONFIG $LDOWN[]_config_args LIBCONFIG_LIBS`])
  AC_CACHE_CHECK([for UP VERSION],
                 [CACHE[]_version],
                 [CACHE[]_version=`$UP[]_CONFIG $LDOWN[]_config_args LIBCONFIG_VERSION`])
  UP[]_CFLAGS="$CACHE[]_cflags"
  UP[]_LIBS="$CACHE[]_libs"
  UP[]_VERSION="$CACHE[]_version"
  LDOWN[]_config_major_version=`echo "$CACHE[]_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  LDOWN[]_config_minor_version=`echo "$CACHE[]_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  LDOWN[]_config_micro_version=`echo "$CACHE[]_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
  LDOWN[]_min_major_version=`echo "$LDOWN[]_min_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
  LDOWN[]_min_minor_version=`echo "$LDOWN[]_min_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
  LDOWN[]_min_micro_version=`echo "$LDOWN[]_min_version" | \
      sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
fi
m4_popdef([LIBCONFIG_CFLAGS])dnl
m4_popdef([LIBCONFIG_LIBS])dnl
m4_popdef([LIBCONFIG_VERSION])dnl
])# _STP_PATH_LIB_CHECK


# _STP_PATH_LIB_FINI(ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
# --------------------------------------------------------
# Finish: report errors and define/substitute variables.  Run any
# user-supplied code for success or failure.  Restore defaults.
AC_DEFUN([_STP_PATH_LIB_FINI],
[dnl define variables and run extra code
UP[]_CFLAGS="$CACHE[]_cflags"
UP[]_LIBS="$CACHE[]_libs"
UP[]_VERSION="$CACHE[]_version"
AC_SUBST(UP[]_CFLAGS)dnl
AC_SUBST(UP[]_LIBS)dnl
AC_SUBST(UP[]_VERSION)dnl
# Run code which depends upon the test result.
if test x$CACHE[]_present = xyes ; then
  m4_if([$1], , :, [$1])     
else
# Restore saved variables if test fails
  UP[]_CFLAGS="$ac_save_[]UP[]_CFLAGS"
  UP[]_LIBS="$ac_save_[]UP[]_LIBS"
  UP[]_VERSION="$ac_save_[]UP[]_VERSION"
  m4_if([$2], , :, [$2])
fi
dnl Restore defaults
STP_PATH_LIB_CHECK_REGISTER_DEFAULTS
STP_PATH_LIB_PKGCONFIG
])# _STP_PATH_LIB_FINI




## -------------------- ##
## 3. Integrity checks. ##
## -------------------- ##


# _STP_PATH_LIB_CHECK_TESTS(MINIMUM-VERSION)
# ------------------------------------------
# Now check if the installed UP is sufficiently new. (Also sanity
# checks the results of DOWN-config to some extent
AC_DEFUN([_STP_PATH_LIB_CHECK_TESTS],
[AC_CACHE_CHECK([for UP - m4_if([$1], ,
                               [any version],
                               [version >= $LDOWN[]_min_version])],
               [CACHE[]_present],
[CACHE[]_present="$LDOWN[]_present_avoidcache"
if test x$CACHE[]_present = xyes -a x$LDOWN[]_test_enabled = xyes -a \
    x$LDOWN[]_version_test_enabled = xyes ; then
  m4_default([_STP_PATH_LIB_CHECK_TEST_COMPILE],
             [_STP_PATH_LIB_CHECK_TEST_COMPILE],
             [_STP_PATH_LIB_CHECK_TEST_COMPILE_DEFAULT])
else
  m4_default([_STP_PATH_LIB_CHECK_VERSION],
             [_STP_PATH_LIB_CHECK_VERSION],
             [_STP_PATH_LIB_CHECK_VERSION_DEFAULT])
# If the user allowed it, try linking with the library
  if test x$LDOWN[]_test_enabled = xyes ; then
    _STP_PATH_LIB_CHECK_LINK(, [
      CACHE[]_present="no"
      if test x$LDOWN[]_version_test_error = xyes ; then
        STP_PATH_LIB_ERROR
      fi
      STP_PATH_LIB_ERROR([The test program failed to compile or link.  See the file])
      STP_PATH_LIB_ERROR([config.log for the exact error that occured.  This usually])
      STP_PATH_LIB_ERROR([means UP was not installed, was incorrectly installed])
      STP_PATH_LIB_ERROR([or that you have moved UP since it was installed.  In])
      STP_PATH_LIB_ERROR([the latter case, you may want to edit the $LDOWN[]_config])
      STP_PATH_LIB_ERROR([script: $UP[]_CONFIG])
    ])
  fi  
fi])
dnl end tests
])# _STP_PATH_LIB_CHECK_TESTS


# _STP_PATH_LIB_CHECK_TEST_COMPILE_DEFAULT
# ----------------------------------------
# Check if the installed UP is sufficiently new. (Also sanity checks
# the results of DOWN-config to some extent.  The test program must
# compile, link and run sucessfully
AC_DEFUN([_STP_PATH_LIB_CHECK_TEST_COMPILE],
[m4_pushdef([RUNLOG], run.[]DOWN[]test)dnl
m4_pushdef([MAJOR], _STP_PATH_LIB_VERSION_PREFIX[]_STP_PATH_LIB_VERSION_MAJOR[]_STP_PATH_LIB_VERSION_SUFFIX)dnl
m4_pushdef([MINOR], _STP_PATH_LIB_VERSION_PREFIX[]_STP_PATH_LIB_VERSION_MINOR[]_STP_PATH_LIB_VERSION_SUFFIX)dnl
m4_pushdef([MICRO], _STP_PATH_LIB_VERSION_PREFIX[]_STP_PATH_LIB_VERSION_MICRO[]_STP_PATH_LIB_VERSION_SUFFIX)dnl
m4_pushdef([DEFMAJOR], _STP_PATH_LIB_DEFVERSION_PREFIX[]_STP_PATH_LIB_DEFVERSION_MAJOR[]_STP_PATH_LIB_DEFVERSION_SUFFIX)dnl
m4_pushdef([DEFMINOR], _STP_PATH_LIB_DEFVERSION_PREFIX[]_STP_PATH_LIB_DEFVERSION_MINOR[]_STP_PATH_LIB_DEFVERSION_SUFFIX)dnl
m4_pushdef([DEFMICRO], _STP_PATH_LIB_DEFVERSION_PREFIX[]_STP_PATH_LIB_DEFVERSION_MICRO[]_STP_PATH_LIB_DEFVERSION_SUFFIX)dnl
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  CFLAGS="$CFLAGS $UP[]_CFLAGS"
  LIBS="$UP[]_LIBS $LIBS"
  rm -f RUNLOG
  AC_TRY_RUN([
#include <$]LDOWN[_header>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;
  FILE *errorlog;

  if ((errorlog = fopen("]ERRORLOG[", "w")) == NULL) {
     exit(1);
   }

  system ("touch ]RUNLOG[");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$]LDOWN[_min_version");
  if (!tmp_version) {
     exit(1);
   }
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     fprintf(errorlog, "*** %s: bad version string\n", "$]LDOWN[_min_version");
     exit(1);
   }

  if ((]MAJOR[ != $]LDOWN[_config_major_version) ||
      (]MINOR[ != $]LDOWN[_config_minor_version) ||
      (]MICRO[ != $]LDOWN[_config_micro_version))
    {
      fprintf(errorlog, "*** '$]UP[_CONFIG ]LIBCONFIG_VERSION[' returned %d.%d.%d, but \n", \
        $]LDOWN[_config_major_version, $]LDOWN[_config_minor_version, \
	$]LDOWN[_config_micro_version);
      fprintf(errorlog, "*** ]UP[ (%d.%d.%d) was found!\n", \
        ]MAJOR[, ]MINOR[, ]MICRO[);
      fprintf(errorlog, "***\n");
      fprintf(errorlog, "*** If $]LDOWN[_config was correct, then it is best to remove\n");
      fprintf(errorlog, "*** the old version of ]UP[.  You may also be able to\n");
      fprintf(errorlog, "*** fix the error by modifying your LD_LIBRARY_PATH enviroment\n");
      fprintf(errorlog, "*** variable, or by editing /etc/ld.so.conf.  Make sure you have\n");
      fprintf(errorlog, "*** run ldconfig if that is required on your system.\n");
      fprintf(errorlog, "*** If $]LDOWN[_config was wrong, set the environment\n");
      fprintf(errorlog, "*** variable ]UP[_CONFIG to point to the correct copy of\n");
      fprintf(errorlog, "*** $]LDOWN[_config, and remove the file config.cache\n");
      fprintf(errorlog, "*** before re-running configure.\n");
    } 
#if defined (]DEFMAJOR[) && defined (]DEFMINOR[) && defined (]DEFMICRO[)
  else if ((]MAJOR[ != ]DEFMAJOR[) ||
	   (]MINOR[ != ]DEFMINOR[) ||
           (]MICRO[ != ]DEFMICRO[))
    {
      fprintf(errorlog, "*** ]UP[ header files (version %d.%d.%d) do not match\n",
	     ]DEFMAJOR[, ]DEFMINOR[, ]DEFMICRO[);
      fprintf(errorlog, "*** library (version %d.%d.%d)\n",
	     ]MAJOR[, ]MINOR[, ]MICRO[);
    }
#endif /* defined (]DEFMAJOR[) ... */
  else
    {
      if ((]MAJOR[ > major) ||
        ((]MAJOR[ == major) && (]MINOR[  > minor)) ||
        ((]MAJOR[ == major) && (]MINOR[ == minor) && (]MICRO[ >= micro)))
      {
        return 0;
       }
     else
      {
        fprintf(errorlog, "*** An old version of ]UP[ (%d.%d.%d) was found.\n",
               ]MAJOR[, ]MINOR[, ]MICRO[);
        fprintf(errorlog, "*** You need a version of ]UP[ newer than %d.%d.%d.\n",
	       major, minor, micro);
        /*fprintf(errorlog, "*** The latest version of ]UP[ is always available from ftp://ftp.my.site\n");*/
        fprintf(errorlog, "***\n");
        fprintf(errorlog, "*** If you have already installed a sufficiently new version, this\n");
        fprintf(errorlog, "*** error probably means that the wrong copy of the $]LDOWN[_config\n");
        fprintf(errorlog, "*** shell script is being found.  The easiest way to fix this is to\n");
        fprintf(errorlog, "*** remove the old version of ]UP[, but you can also set the\n");
        fprintf(errorlog, "*** ]UP[_CONFIG environment variable to point to the correct\n");
	fprintf(errorlog, "*** copy of $]LDOWN[_config.  (In this case, you will have to\n");
        fprintf(errorlog, "*** modify your LD_LIBRARY_PATH environment variable, or edit\n");
        fprintf(errorlog, "*** /etc/ld.so.conf so that the correct libraries are found at\n");
	fprintf(errorlog, "*** run-time.)\n");
      }
    }
  return 1;
}
],, [CACHE[]_present="no"],
    [STP_PATH_LIB_ERROR([cross compiling; assumed OK.])])
  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"

if test -f RUNLOG ; then
  :
elif test x$LDOWN[]_version_test_enabled = xyes ; then
  STP_PATH_LIB_ERROR([Could not run UP test program, checking why...])
  STP_PATH_LIB_ERROR
  _STP_PATH_LIB_CHECK_LINK(dnl
    [STP_PATH_LIB_ERROR([The test program compiled, but did not run.  This usually])
     STP_PATH_LIB_ERROR([means that the run-time linker is not finding UP or finding])
     STP_PATH_LIB_ERROR([finding the wrong version of UP.  If it is not finding])
     STP_PATH_LIB_ERROR([UP, you will need to set your LD_LIBRARY_PATH environment])
     STP_PATH_LIB_ERROR([variable, or edit /etc/ld.so.conf to point to the installed])
     STP_PATH_LIB_ERROR([location.  Also, make sure you have run ldconfig if that is])
     STP_PATH_LIB_ERROR([required on your system.])
     STP_PATH_LIB_ERROR
     STP_PATH_LIB_ERROR([If you have an old version installed, it is best to remove])
     STP_PATH_LIB_ERROR([it, although you may also be able to get things to work by])
     STP_PATH_LIB_ERROR([modifying LD_LIBRARY_PATH])
    ],
    [STP_PATH_LIB_ERROR([The test program failed to compile or link.  See the file])
     STP_PATH_LIB_ERROR([config.log for the exact error that occured.  This usually])
     STP_PATH_LIB_ERROR([means UP was incorrectly installed or that you have])
     STP_PATH_LIB_ERROR([moved UP since it was installed.  In the latter case,])
     STP_PATH_LIB_ERROR([you may want to edit the $LDOWN[]_config script:])
     STP_PATH_LIB_ERROR([$UP[]_CONFIG])
    ])
fi
rm -f RUNLOG
m4_popdef([RUNLOG])dnl
m4_popdef([MAJOR])dnl
m4_popdef([MINOR])dnl
m4_popdef([MICRO])dnl
m4_popdef([DEFMAJOR])dnl
m4_popdef([DEFMINOR])dnl
m4_popdef([DEFMICRO])dnl
])# _STP_PATH_LIB_CHECK_TEST_COMPILE_DEFAULT


# _STP_PATH_LIB_CHECK_VERSION_DEFAULT
# -----------------------------------
# Check that the library version (config) is greater than or equal to
# the requested (minimum) version.
AC_DEFUN([_STP_PATH_LIB_CHECK_VERSION],
[m4_if([$2], , ,
       [if test x$LDOWN[]_present_avoidcache = xyes ; then
         if test \
             "$LDOWN[]_config_major_version" -lt "$LDOWN[]_min_major_version" -o \
             "$LDOWN[]_config_major_version" -eq "$LDOWN[]_min_major_version" -a \
             "$LDOWN[]_config_minor_version" -lt "$LDOWN[]_min_minor_version" -o \
             "$LDOWN[]_config_major_version" -eq "$LDOWN[]_min_major_version" -a \
             "$LDOWN[]_config_minor_version" -eq "$LDOWN[]_min_minor_version" -a \
             "$LDOWN[]_config_micro_version" -lt "$LDOWN[]_min_micro_version" ; then
           CACHE[]_present="no"
           LDOWN[]_version_test_error="yes"
           STP_PATH_LIB_ERROR([$UP[]_CONFIG --version' returned $LDOWN[]_config_major_version.$LDOWN[]_config_minor_version.$LDOWN[]_config_micro_version, but])
           STP_PATH_LIB_ERROR([UP (>= $LDOWN[]_min_version) was needed.])
           STP_PATH_LIB_ERROR
           STP_PATH_LIB_ERROR([If $]LDOWN[_config was wrong, set the environment])
           STP_PATH_LIB_ERROR([variable ]UP[_CONFIG to point to the correct copy of])
           STP_PATH_LIB_ERROR([$]LDOWN[_config, and remove the file config.cache])
           STP_PATH_LIB_ERROR([before re-running configure.])
         else
           CACHE[]_present="yes"
         fi
       fi])
])# _STP_PATH_LIB_CHECK_VERSION_DEFAULT


# _STP_PATH_LIB_CHECK_LINK_DEFAULT(SUCCESS, FAIL)
# ---------------------------------------
# Check if the library will link successfully.  If specified, run
# SUCCESS or FAIL on success or failure
AC_DEFUN([_STP_PATH_LIB_CHECK_LINK],
[ac_save_CFLAGS="$CFLAGS"
ac_save_LIBS="$LIBS"
CFLAGS="$CFLAGS $UP[]_CFLAGS"
LIBS="$LIBS $UP[]_LIBS"
AC_TRY_LINK([ #include <stdio.h> ], ,
            [m4_if([$1], , :, [$1])],
            [m4_if([$2], , :, [$2])])
CFLAGS="$ac_save_CFLAGS"
LIBS="$ac_save_LIBS"
])# _STP_PATH_LIB_CHECK_LINK_DEFAULT




## ------------------- ##
## 4. Error reporting. ##
## ------------------- ##


# STP_PATH_LIB_ERROR(MESSAGE)
# ---------------------------
# Print an error message, MESSAGE, to the error log.
AC_DEFUN([STP_PATH_LIB_ERROR],
[echo '*** m4_if([$1], , , [$1])' >>ERRORLOG])


# _STP_PATH_LIB_ERROR_DUMP
# ------------------------
# Print the error log (after main AC_CACHE_CHECK completes).
AC_DEFUN([_STP_PATH_LIB_ERROR_DUMP],
[if test -f ERRORLOG ; then
  cat ERRORLOG
  rm -f ERRORLOG
fi])




## ------------------ ##
## 5. Feature macros. ##
## ------------------ ##


# STP_PATH_LIB_PKGCONFIG
# ----------------------
# Enable pkgconfig support in libconfig script (default).
AC_DEFUN([STP_PATH_LIB_PKGCONFIG],
[m4_define([STP_PATH_LIB_USEPKGCONFIG], [true])
])dnl


# STP_PATH_LIB_LIBCONFIG
# ----------------------
# Disable pkgconfig support in libconfig script.
AC_DEFUN([STP_PATH_LIB_LIBCONFIG],
[m4_define([STP_PATH_LIB_USEPKGCONFIG], [false])
])dnl


# STP_PATH_LIB_REGISTER (MACRO, REPLACEMENT)
# ------------------------------------------------
# Register a macro to replace the default checks.  Use the REPLACEMENT
# macro for the check macro MACRO.
#
# Possible MACROs are:
#   _STP_PATH_LIB_CHECK_COMPILE and
#   _STP_PATH_LIP_CHECK_VERSION
# You should make sure that replacement macros use the same arguments
# (if any), and do error logging in the same manner and behave in the
# same way as the original.

# Non-default header names may be specified, as well as version
# variable names in the library itself (needed for
# _STP_PATH_LIB_CHECK_COMPILE):
#   _STP_PATH_LIB_HEADER
#   _STP_PATH_LIB_VERSION_PREFIX (default library_)
#   _STP_PATH_LIB_VERSION_MAJOR (default major)
#   _STP_PATH_LIB_VERSION_MINOR (default minor)
#   _STP_PATH_LIB_VERSION_MICRO (default micro)
#   _STP_PATH_LIB_VERSION_SUFFIX (default _version)
#   _STP_PATH_LIB_DEFVERSION_PREFIX (default LIBRARY_)
#   _STP_PATH_LIB_DEFVERSION_MAJOR (default MAJOR)
#   _STP_PATH_LIB_DEFVERSION_MINOR (default MINOR)
#   _STP_PATH_LIB_DEFVERSION_MICRO (default MICRO)
#   _STP_PATH_LIB_DEFVERSION_SUFFIX (default _VERSION)
# For example, library_major_version.
AC_DEFUN([STP_PATH_LIB_REGISTER],
[m4_define([$1], [$2])])


# STP_PATH_LIB_CHECK_REGISTER_DEFAULTS
# ------------------------------------
# Restore the default check macros.
AC_DEFUN([STP_PATH_LIB_CHECK_REGISTER_DEFAULTS],
[_STP_PATH_LIB_CHECK_REGISTER_DEFAULTS([_STP_PATH_LIB_CHECK_COMPILE],
                                       [_STP_PATH_LIP_CHECK_VERSION],
                                       [_STP_PATH_LIB_HEADER],
                                       [_STP_PATH_LIB_VERSION_PREFIX],
                                       [_STP_PATH_LIB_VERSION_MAJOR],
                                       [_STP_PATH_LIB_VERSION_MINOR],
                                       [_STP_PATH_LIB_VERSION_MICRO],
                                       [_STP_PATH_LIB_VERSION_SUFFIX],
                                       [_STP_PATH_LIB_DEFVERSION_PREFIX],
                                       [_STP_PATH_LIB_DEFVERSION_MAJOR],
                                       [_STP_PATH_LIB_DEFVERSION_MINOR],
                                       [_STP_PATH_LIB_DEFVERSION_MICRO],
                                       [_STP_PATH_LIB_DEFVERSION_SUFFIX])
])# STP_PATH_LIB_CHECK_REGISTER_DEFAULTS


# _STP_PATH_LIB_CHECK_REGISTER_DEFAULTS(MACROs ...)
# --------------------------------------------
# Undefine MACROs.
AC_DEFUN([STP_PATH_LIB_CHECK_REGISTER_DEFAULTS],
[m4_if([$1], , ,
       [m4_ifdef([$1],
                 [m4_undefine([$1])])
        STP_PATH_LIB_CHECK_REGISTER_DEFAULTS(m4_shift($@))       
       ])
])
