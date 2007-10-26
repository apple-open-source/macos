# ltdl.m4 - Configure ltdl for the target system. -*-Autoconf-*-
# Copyright (C) 1999-2006 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 10 LTDL_INIT

# LT_CONFIG_LTDL_DIR(DIRECTORY, [LTDL-MODE])
# ------------------------------------------
# DIRECTORY contains the libltdl sources.  It is okay to call this
# function multiple times, as long as the same DIRECTORY is always given.
AC_DEFUN([LT_CONFIG_LTDL_DIR],
[AC_BEFORE([$0], [LT_WITH_LTDL])
_$0($*)
])# LT_CONFIG_LTDL_DIR

# We break this out into a separate macro, so that we can call it safely
# internally without being caught accidentally by the sed scan in libtoolize.
m4_defun([_LT_CONFIG_LTDL_DIR],
[dnl remove trailing slashes
m4_pushdef([_ARG_DIR], m4_bpatsubst([$1], [/*$]))
m4_case(_LTDL_DIR,
	[], [dnl only set lt_ltdl_dir if _ARG_DIR is not simply `.'
	     m4_if(_ARG_DIR, [.],
	             [],
		 [m4_define([_LTDL_DIR], _ARG_DIR)
	          _LT_SHELL_INIT([lt_ltdl_dir=']_ARG_DIR['])])],
    [m4_if(_ARG_DIR, _LTDL_DIR,
	    [],
	[m4_fatal([multiple libltdl directories: `]_LTDL_DIR[', `]_ARG_DIR['])])])
m4_popdef([_ARG_DIR])
dnl If not otherwise defined, default to the 1.5.x compatible subproject mode:
m4_if(_LTDL_MODE, [],
	[m4_define([_LTDL_MODE], m4_default([$2], [subproject]))
	m4_if([-1], [m4_bregexp(_LTDL_MODE, [\(subproject\|\(non\)?recursive\)])],
		[m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])
])# LT_CONFIG_LTDL_DIR

# Initialise:
m4_define([_LTDL_DIR], [])
m4_define([_LTDL_MODE], [])


# LTDL_CONVENIENCE
# ----------------
# sets LIBLTDL to the link flags for the libltdl convenience library and
# LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-convenience to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called here.  LIBLTDL will be prefixed with
# '${top_builddir}/' and LTDLINCL will be prefixed with '${top_srcdir}/'
# (note the single quotes!).  If your package is not flat and you're not
# using automake, define top_builddir and top_srcdir appropriately in
# your Makefiles.
AC_DEFUN([LTDL_CONVENIENCE],
[AC_BEFORE([$0], [LT_WITH_LTDL])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_CONVENIENCE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl

case $enable_ltdl_convenience in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
  esac
LIBLTDL='${top_builddir}'"${lt_ltdl_dir+/$lt_ltdl_dir}/libltdlc.la"
LTDLINCL='-I${top_srcdir}'"${lt_ltdl_dir+/$lt_ltdl_dir}"

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL="$LTDLINCL"
AC_SUBST([INCLTDL])
])# LTDL_CONVENIENCE

# AC_LIBLTDL_CONVENIENCE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_CONVENIENCE],
[_LT_CONFIG_LTDL_DIR([$1])
LTDL_CONVENIENCE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_CONVENIENCE], [])


# LTDL_INSTALLABLE
# ----------------
# sets LIBLTDL to the link flags for the libltdl installable library
# and LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-install to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called from here.  If an installed libltdl
# is not found, LIBLTDL will be prefixed with '${top_builddir}/'
# and LTDLINCL will be prefixed with '${top_srcdir}/' (note the single
# quotes!).  If your package is not flat and you're not using automake,
# define top_builddir and top_srcdir appropriately in your Makefiles.
# In the future, this macro may have to be called after LT_INIT.
AC_DEFUN([LTDL_INSTALLABLE],
[AC_BEFORE([$0], [LT_WITH_LTDL])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_INSTALLABLE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl

AC_CHECK_LIB([ltdl], [lt_dlinit],
  [test x"$enable_ltdl_install" != xyes && enable_ltdl_install=no],
  [if test x"$enable_ltdl_install" = xno; then
     AC_MSG_WARN([libltdl not installed, but installation disabled])
   else
     enable_ltdl_install=yes
   fi
  ])
if test x"$enable_ltdl_install" = x"yes"; then
  ac_configure_args="$ac_configure_args --enable-ltdl-install"
  LIBLTDL='${top_builddir}'"${lt_ltdl_dir+/$lt_ltdl_dir}/libltdl.la"
  LTDLINCL='-I${top_srcdir}'"${lt_ltdl_dir+/$lt_ltdl_dir}"
else
  ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
  LIBLTDL="-lltdl"
  LTDLINCL=
fi

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL="$LTDLINCL"
AC_SUBST([INCLTDL])
])# LTDL_INSTALLABLE

# AC_LIBLTDL_INSTALLABLE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_INSTALLABLE],
[_LT_CONFIG_LTDL_DIR([$1])
LTDL_INSTALLABLE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_INSTALLABLE], [])


# _LTDL_MODE_DISPATCH
# -------------------
m4_define([_LTDL_MODE_DISPATCH],
[dnl If _LTDL_DIR is `.', then we are configuring libltdl itself:
m4_if(_LTDL_DIR, [],
	[],
    dnl if _LTDL_MODE was not set already, the default value is `subproject':
    [m4_case(m4_default(_LTDL_MODE, [subproject]),
	  [subproject], [AC_CONFIG_SUBDIRS(_LTDL_DIR)
			  _LT_SHELL_INIT([lt_dlopen_dir="$lt_ltdl_dir"])],
	  [nonrecursive], [_LT_SHELL_INIT([lt_dlopen_dir="$lt_ltdl_dir"])],
	  [recursive], [],
	[m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])dnl
dnl Be careful not to expand twice:
m4_define([$0], [])
])# _LTDL_MODE_DISPATCH


# LT_WITH_LTDL
# ------------
# Clients of libltdl can use this macro to allow the installer to
# choose between a shipped copy of the ltdl sources or a preinstalled
# version of the library.  If the shipped ltdl sources are not in a
# subdirectory named libltdl, the directory name must be given by
# LT_CONFIG_LTDL_DIR.
AC_DEFUN([LT_WITH_LTDL],
[dnl If neither LT_CONFIG_LTDL_DIR, LTDL_CONVENIENCE nor LTDL_INSTALLABLE
dnl was called yet, then for old times' sake, we assume libltdl is in an
dnl eponymous directory:
AC_PROVIDE_IFELSE([LT_CONFIG_LTDL_DIR], [], [_LT_CONFIG_LTDL_DIR([libltdl])])

AC_ARG_WITH([included_ltdl],
    [AS_HELP_STRING([--with-included-ltdl],
                    [use the GNU ltdl sources included here])])

if test "x$with_included_ltdl" != xyes; then
  # We are not being forced to use the included libltdl sources, so
  # decide whether there is a useful installed version we can use.
  lt_dlinterface_register_found=no
  AC_CHECK_HEADER([ltdl.h],
      [AC_CHECK_LIB([ltdl], [lt_dlinterface_register],
          [with_included_ltdl=no],
          [with_included_ltdl=yes])],

      [],
      [AC_INCLUDES_DEFAULT]
  )
  AC_MSG_CHECKING([for lt_dlinterface_register in ltdl.h])
  test "x$with_included_ltdl" = xno && lt_dlinterface_register_found=yes
  AC_MSG_RESULT([$lt_dlinterface_register_found])
fi

if test "x$enable_ltdl_install" != xyes; then
  # If the user did not specify an installable libltdl, then default
  # to a convenience lib.
  LTDL_CONVENIENCE
fi

if test "x$with_included_ltdl" = xno; then
  # If the included ltdl is not to be used, then use the
  # preinstalled libltdl we found.
  AC_DEFINE([HAVE_LTDL], [1],
    [Define this if a modern libltdl is already installed])
  LIBLTDL=-lltdl
  LTDLINCL=
  INCLTDL=
fi

# Report our decision...
AC_MSG_CHECKING([whether to use included libltdl])
AC_MSG_RESULT([$with_included_ltdl])

dnl Be certain that LTDL_INIT is invoked if we are configuring libltdl
dnl from here:
m4_if(_LTDL_MODE, [subproject],
	[_LTDL_MODE_DISPATCH],
    [AC_PROVIDE_IFELSE([LTDL_INIT],
	    [],
	[LTDL_INIT
	AC_DEFUN([LTDL_INIT], [])])])
])# LT_WITH_LTDL

# Old name:
AU_ALIAS([AC_WITH_LTDL], [LT_WITH_LTDL])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_WITH_LTDL], [])


# LTDL_INIT
# ---------
# Perform all the checks necessary for compilation of the ltdl objects
#  -- including compiler checks and header checks.  This is a public
# interface  mainly for the benefit of libltdl's own configure.ac, most
# other users should call LT_WITH_LTDL instead.
AC_DEFUN([LTDL_INIT],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([LT_SYS_MODULE_EXT])dnl
AC_REQUIRE([LT_SYS_MODULE_PATH])dnl
AC_REQUIRE([LT_SYS_DLSEARCH_PATH])dnl
AC_REQUIRE([LT_LIB_DLLOAD])dnl
AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl
AC_REQUIRE([LT_FUNC_DLSYM_USCORE])dnl
AC_REQUIRE([LT_SYS_DLOPEN_DEPLIBS])dnl
AC_REQUIRE([gl_FUNC_ARGZ])dnl

m4_require([_LT_CHECK_OBJDIR])dnl
m4_require([_LT_HEADER_DLFCN])dnl
m4_require([_LT_CHECK_DLPREOPEN])dnl
m4_require([_LT_DECL_SED])dnl

dnl Don't require this, or it will be expanded earlier than the code
dnl that sets the variables it relies on:
_LT_ENABLE_INSTALL

dnl Although deprecated and no longer documented, alpha releases of
dnl libtool used to define an LTDL_INIT to take a DIRECTORY orgument.
dnl If LT_CONFIG_LTDL_DIR was called already, but LTDL_INIT was given a
dnl DIRECTORY argument, check it is the same as previous invocations.  If
dnl it still hasn't been called, then do it now, defaulting to `libltdl'
dnl if no DIRECTORY argument was passed.
m4_provide_if([_LT_CONFIG_LTDL_DIR],
	[m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])],
    [_LT_CONFIG_LTDL_DIR(m4_default([$1], [libltdl]))])dnl

dnl _LTDL_MODE specific code must be evaluated at least once:
_LTDL_MODE_DISPATCH

# In order that ltdl.c can compile, find out the first AC_CONFIG_HEADERS
# the user used.  This is so that ltdl.h can pick up the parent projects
# config.h file, The first file in AC_CONFIG_HEADERS must contain the
# definitions required by ltdl.c.
# FIXME: Remove use of undocumented AC_LIST_HEADERS (2.59 compatibility).
AC_CONFIG_COMMANDS_PRE([dnl
m4_pattern_allow([^LT_CONFIG_H$])dnl
m4_ifset([AH_HEADER],
    [LT_CONFIG_H=AH_HEADER],
    [m4_ifset([AC_LIST_HEADERS],
	    [LT_CONFIG_H=`echo "AC_LIST_HEADERS" | $SED 's,^[[      ]]*,,;s,[[ :]].*$,,'`],
	[])])])
AC_SUBST([LT_CONFIG_H])

AC_CHECK_HEADERS([memory.h unistd.h dl.h sys/dl.h dld.h mach-o/dyld.h dirent.h],
	[], [], [AC_INCLUDES_DEFAULT])
AC_CHECK_HEADERS([string.h strings.h], [break], [], [AC_INCLUDES_DEFAULT])

AC_CHECK_FUNCS([closedir opendir readdir], [], [AC_LIBOBJ([lt__dirent])])
AC_CHECK_FUNCS([strlcat strlcpy], [], [AC_LIBOBJ([lt__strl])])

name=ltdl
LTDLOPEN=`eval "\\$ECHO \"$libname_spec\""`
AC_SUBST([LTDLOPEN])
])# LTDL_INIT

# Old name:
AU_ALIAS([AC_LIB_LTDL], [LTDL_INIT])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIB_LTDL], [])


# _LT_ENABLE_INSTALL
# ------------------
m4_define([_LT_ENABLE_INSTALL],
[AC_ARG_ENABLE([ltdl-install],
    [AS_HELP_STRING([--enable-ltdl-install], [install libltdl])])

AM_CONDITIONAL(INSTALL_LTDL, test x"${enable_ltdl_install-no}" != xno)
AM_CONDITIONAL(CONVENIENCE_LTDL, test x"${enable_ltdl_convenience-no}" != xno)
])# _LT_ENABLE_INSTALL


# LT_SYS_DLOPEN_DEPLIBS
# ---------------------
AC_DEFUN([LT_SYS_DLOPEN_DEPLIBS],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_CACHE_CHECK([whether deplibs are loaded by dlopen],
  [lt_cv_sys_dlopen_deplibs],
  [# PORTME does your system automatically load deplibs for dlopen?
  # or its logical equivalent (e.g. shl_load for HP-UX < 11)
  # For now, we just catch OSes we know something about -- in the
  # future, we'll try test this programmatically.
  lt_cv_sys_dlopen_deplibs=unknown
  case $host_os in
  aix3*|aix4.1.*|aix4.2.*)
    # Unknown whether this is true for these versions of AIX, but
    # we want this `case' here to explicitly catch those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  aix[[45]]*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  darwin*)
    # Assuming the user has installed a libdl from somewhere, this is true
    # If you are looking for one http://www.opendarwin.org/projects/dlcompat
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  freebsd* | dragonfly*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  gnu* | linux* | k*bsd*-gnu)
    # GNU and its variants, using gnu ld.so (Glibc)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  hpux10*|hpux11*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  interix*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  irix[[12345]]*|irix6.[[01]]*)
    # Catch all versions of IRIX before 6.2, and indicate that we don't
    # know how it worked for any of those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  irix*)
    # The case above catches anything before 6.2, and it's known that
    # at 6.2 and later dlopen does load deplibs.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  netbsd*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  openbsd*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  osf[[1234]]*)
    # dlopen did load deplibs (at least at 4.x), but until the 5.x series,
    # it did *not* use an RPATH in a shared library to find objects the
    # library depends on, so we explicitly say `no'.
    lt_cv_sys_dlopen_deplibs=no
    ;;
  osf5.0|osf5.0a|osf5.1)
    # dlopen *does* load deplibs and with the right loader patch applied
    # it even uses RPATH in a shared library to search for shared objects
    # that the library depends on, but there's no easy way to know if that
    # patch is installed.  Since this is the case, all we can really
    # say is unknown -- it depends on the patch being installed.  If
    # it is, this changes to `yes'.  Without it, it would be `no'.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  osf*)
    # the two cases above should catch all versions of osf <= 5.1.  Read
    # the comments above for what we know about them.
    # At > 5.1, deplibs are loaded *and* any RPATH in a shared library
    # is used to find them so we can finally say `yes'.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  qnx*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  solaris*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX* | sysv4*uw2*)
    libltdl_cv_sys_dlopen_deplibs=yes
    ;;
  esac
  ])
if test "$lt_cv_sys_dlopen_deplibs" != yes; then
 AC_DEFINE([LTDL_DLOPEN_DEPLIBS], [1],
    [Define if the OS needs help to load dependent libraries for dlopen().])
fi
])# LT_SYS_DLOPEN_DEPLIBS

# Old name:
AU_ALIAS([AC_LTDL_SYS_DLOPEN_DEPLIBS], [LT_SYS_DLOPEN_DEPLIBS])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYS_DLOPEN_DEPLIBS], [])


# LT_SYS_MODULE_EXT
# -----------------
AC_DEFUN([LT_SYS_MODULE_EXT],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([which extension is used for runtime loadable modules],
  [libltdl_cv_shlibext],
[
module=yes
eval libltdl_cv_shlibext=$shrext_cmds
  ])
if test -n "$libltdl_cv_shlibext"; then
  m4_pattern_allow([LT_MODULE_EXT])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_EXT], ["$libltdl_cv_shlibext"],
    [Define to the extension used for runtime loadable modules, say, ".so".])
fi
])# LT_SYS_MODULE_EXT

# Old name:
AU_ALIAS([AC_LTDL_SHLIBEXT], [LT_SYS_MODULE_EXT])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBEXT], [])


# LT_SYS_MODULE_PATH
# ------------------
AC_DEFUN([LT_SYS_MODULE_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([which variable specifies run-time module search path],
  [lt_cv_module_path_var], [lt_cv_module_path_var="$shlibpath_var"])
if test -n "$lt_cv_module_path_var"; then
  m4_pattern_allow([LT_MODULE_PATH_VAR])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_PATH_VAR], ["$lt_cv_module_path_var"],
    [Define to the name of the environment variable that determines the run-time module search path.])
fi
])# LT_SYS_MODULE_PATH

# Old name:
AU_ALIAS([AC_LTDL_SHLIBPATH], [LT_SYS_MODULE_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBPATH], [])


# LT_SYS_DLSEARCH_PATH
# --------------------
AC_DEFUN([LT_SYS_DLSEARCH_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([for the default library search path],
  [lt_cv_sys_dlsearch_path],
  [lt_cv_sys_dlsearch_path="$sys_lib_dlsearch_path_spec"])
if test -n "$lt_cv_sys_dlsearch_path"; then
  sys_dlsearch_path=
  for dir in $lt_cv_sys_dlsearch_path; do
    if test -z "$sys_dlsearch_path"; then
      sys_dlsearch_path="$dir"
    else
      sys_dlsearch_path="$sys_dlsearch_path$PATH_SEPARATOR$dir"
    fi
  done
  m4_pattern_allow([LT_DLSEARCH_PATH])dnl
  AC_DEFINE_UNQUOTED([LT_DLSEARCH_PATH], ["$sys_dlsearch_path"],
    [Define to the system default library search path.])
fi
])# LT_SYS_DLSEARCH_PATH

# Old name:
AU_ALIAS([AC_LTDL_SYSSEARCHPATH], [LT_SYS_DLSEARCH_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYSSEARCHPATH], [])


# _LT_CHECK_DLPREOPEN
# -------------------
m4_defun([_LT_CHECK_DLPREOPEN],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([whether libtool supports -dlopen/-dlpreopen],
  [libltdl_cv_preloaded_symbols],
  [if test -n "$lt_cv_sys_global_symbol_pipe"; then
    libltdl_cv_preloaded_symbols=yes
  else
    libltdl_cv_preloaded_symbols=no
  fi
  ])
if test x"$libltdl_cv_preloaded_symbols" = xyes; then
  AC_DEFINE([HAVE_PRELOADED_SYMBOLS], [1],
    [Define if libtool can extract symbol lists from object files.])
fi
])# _LT_CHECK_DLPREOPEN


# LT_LIB_DLLOAD
# -------------
AC_DEFUN([LT_LIB_DLLOAD],
[m4_pattern_allow([^LT_DLLOADERS$])
LT_DLLOADERS=
AC_SUBST([LT_DLLOADERS])

AC_LANG_PUSH([C])

LIBADD_DLOPEN=
AC_SEARCH_LIBS([dlopen], [dl],
	[AC_DEFINE([HAVE_LIBDL], [1],
		   [Define if you have the libdl library or equivalent.])
	if test "$ac_cv_search_dlopen" != "none required" ; then
	  LIBADD_DLOPEN="-ldl"
	fi
	libltdl_cv_lib_dl_dlopen="yes"
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#if HAVE_DLFCN_H
#  include <dlfcn.h>
#endif
    ]], [[dlopen(0, 0);]])],
	    [AC_DEFINE([HAVE_LIBDL], [1],
		       [Define if you have the libdl library or equivalent.])
	    libltdl_cv_func_dlopen="yes"
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
	[AC_CHECK_LIB([svld], [dlopen],
		[AC_DEFINE([HAVE_LIBDL], [1],
			 [Define if you have the libdl library or equivalent.])
	        LIBADD_DLOPEN="-lsvld" libltdl_cv_func_dlopen="yes"
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"])])])
if test x"$libltdl_cv_func_dlopen" = xyes || test x"$libltdl_cv_lib_dl_dlopen" = xyes
then
  lt_save_LIBS="$LIBS"
  LIBS="$LIBS $LIBADD_DLOPEN"
  AC_CHECK_FUNCS([dlerror])
  LIBS="$lt_save_LIBS"
fi
AC_SUBST([LIBADD_DLOPEN])

LIBADD_SHL_LOAD=
AC_CHECK_FUNC([shl_load],
	[AC_DEFINE([HAVE_SHL_LOAD], [1],
		   [Define if you have the shl_load function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"],
    [AC_CHECK_LIB([dld], [shl_load],
	    [AC_DEFINE([HAVE_SHL_LOAD], [1],
		       [Define if you have the shl_load function.])
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"
	    LIBADD_SHL_LOAD="-ldld"])])
AC_SUBST([LIBADD_SHL_LOAD])

case $host_os in
darwin[[1567]].*)
# We only want this for pre-Mac OS X 10.4.
  AC_CHECK_FUNC([_dyld_func_lookup],
	[AC_DEFINE([HAVE_DYLD], [1],
		   [Define if you have the _dyld_func_lookup function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dyld.la"])
  ;;
beos*)
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}load_add_on.la"
  ;;
cygwin* | mingw* | os2* | pw32*)
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}loadlibrary.la"
  ;;
esac

AC_CHECK_LIB([dld], [dld_link],
	[AC_DEFINE([HAVE_DLD], [1],
		   [Define if you have the GNU dld library.])
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dld_link.la"])
AC_SUBST([LIBADD_DLD_LINK])

m4_pattern_allow([^LT_DLPREOPEN$])
LT_DLPREOPEN=
if test -n "$LT_DLLOADERS"
then
  for lt_loader in $LT_DLLOADERS; do
    LT_DLPREOPEN="$LT_DLPREOPEN-dlpreopen $lt_loader "
  done
  AC_DEFINE([HAVE_LIBDLLOADER], [1],
            [Define if libdlloader will be built on this platform])
fi
AC_SUBST([LT_DLPREOPEN])

dnl This isn't used anymore, but set it for backwards compatibility
LIBADD_DL="$LIBADD_DLOPEN $LIBADD_SHL_LOAD"
AC_SUBST([LIBADD_DL])

AC_LANG_POP
])# LT_LIB_DLLOAD

# Old name:
AU_ALIAS([AC_LTDL_DLLIB], [LT_LIB_DLLOAD])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLLIB], [])


# LT_SYS_SYMBOL_USCORE
# --------------------
# does the compiler prefix global symbols with an underscore?
AC_DEFUN([LT_SYS_SYMBOL_USCORE],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([for _ prefix in compiled symbols],
  [lt_cv_sys_symbol_underscore],
  [lt_cv_sys_symbol_underscore=no
  cat > conftest.$ac_ext <<_LT_EOF
void nm_test_func(){}
int main(){nm_test_func;return 0;}
_LT_EOF
  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    ac_nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| $lt_cv_sys_global_symbol_pipe \> $ac_nlist) && test -s "$ac_nlist"; then
      # See whether the symbols have a leading underscore.
      if grep '^. _nm_test_func' "$ac_nlist" >/dev/null; then
        lt_cv_sys_symbol_underscore=yes
      else
        if grep '^. nm_test_func ' "$ac_nlist" >/dev/null; then
	  :
        else
	  echo "configure: cannot find nm_test_func in $ac_nlist" >&AS_MESSAGE_LOG_FD
        fi
      fi
    else
      echo "configure: cannot run $lt_cv_sys_global_symbol_pipe" >&AS_MESSAGE_LOG_FD
    fi
  else
    echo "configure: failed program was:" >&AS_MESSAGE_LOG_FD
    cat conftest.c >&AS_MESSAGE_LOG_FD
  fi
  rm -rf conftest*
  ])
  sys_symbol_underscore=$lt_cv_sys_symbol_underscore
  AC_SUBST([sys_symbol_underscore])
])# LT_SYS_SYMBOL_USCORE

# Old name:
AU_ALIAS([AC_LTDL_SYMBOL_USCORE], [LT_SYS_SYMBOL_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYMBOL_USCORE], [])


# LT_FUNC_DLSYM_USCORE
# --------------------
AC_DEFUN([LT_FUNC_DLSYM_USCORE],
[AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl
if test x"$lt_cv_sys_symbol_underscore" = xyes; then
  if test x"$libltdl_cv_func_dlopen" = xyes ||
     test x"$libltdl_cv_lib_dl_dlopen" = xyes ; then
	AC_CACHE_CHECK([whether we have to add an underscore for dlsym],
	  [libltdl_cv_need_uscore],
	  [libltdl_cv_need_uscore=unknown
          save_LIBS="$LIBS"
          LIBS="$LIBS $LIBADD_DLOPEN"
	  _LT_TRY_DLOPEN_SELF(
	    [libltdl_cv_need_uscore=no], [libltdl_cv_need_uscore=yes],
	    [],				 [libltdl_cv_need_uscore=cross])
	  LIBS="$save_LIBS"
	])
  fi
fi

if test x"$libltdl_cv_need_uscore" = xyes; then
  AC_DEFINE([NEED_USCORE], [1],
    [Define if dlsym() requires a leading underscore in symbol names.])
fi
])# LT_FUNC_DLSYM_USCORE

# Old name:
AU_ALIAS([AC_LTDL_DLSYM_USCORE], [LT_FUNC_DLSYM_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLSYM_USCORE], [])
