# CYGNUS LOCAL
#
# This entire file is Cygnus local, it contains a set of cross
# platform autoconf macros to be used by Tcl extensions.

# FIXME: There seems to be a problem with variable
# names that still need an expansion (like $foo_FILE)
# since another eval might be needed in these macros.

#--------------------------------------------------------------------
# TCL_TOOL_PATH
#
#	Return a file path that the build system tool will understand.
#	This path might be different than the path used in the
#	Makefiles.
#
# Arguments:
#	
#	VAR
#	PATH
#	
# Results:
#
#
# Example:
#
# TCL_TOOL_PATH(TCL_CC_PATH, /usr/local/compiler)
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_PATH, [
  val=$2

  if test "$val" = "" ; then
    AC_MSG_ERROR([Empty value for variable $1])
  fi

  case "${host}" in
    *windows32* | *mingw32*)
      if test "${CYGPATH}" = ""; then
        AC_MSG_ERROR([CYGPATH variable is not defined.])
      elif test "${CYGPATH}" = "echo"; then
        # No cygpath when cross compiling
        $1=$val
      else
        # store literal argument text in a variable
        val=$val
        # Convert Cygwin to Windows path (/tmp/foo -> C:\Tmp\foo)
        val="`${CYGPATH} $val`"
        # Convert path like C:\Tmp\foo to C:/Tmp/foo
        $1="`echo $val | sed -e s#\\\\\\\\#/#g`"
      fi
    ;;
    *)
      # Default to a no-op under Unix or Cygwin gcc
      $1=$val
    ;;
  esac
])

# FIXME: It would simplify things if no SUFFIX had to be passed
# into these LONGNAME macros. Using the TCL_SHARED_LIB_SUFFIX
# and TCL_UNSHARED_LIB_SUFFIX from tclConfig.sh might do the trick!

#--------------------------------------------------------------------
# TCL_TOOL_STATIC_LIB_LONGNAME
#
#	Return static library name in the "long format" understood by
#	the build tools. This might involve prepending a suffix
#	and appending version information to the library name.
#
# Arguments:
#	
#	VAR
#	LIBNAME
#	SUFFIX
#	
# Depends on:
#	TCL_DBGX
#	TCL_VENDOR_PREFIX
#
# Example:
#
# TCL_TOOL_STATIC_LIB_LONGNAME(TCL_LIB, tcl, $TCL_UNSHARED_LIB_SUFFIX)
#
# Results:
#
#	TCL_LIB=libtcl83.a
#
#	or
#
#	TCL_LIB=tcl83.lib
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_STATIC_LIB_LONGNAME, [
  libname=$2
  suffix=$3

  case "${host}" in
    *windows32* | *mingw32*)
      if test "$GCC" != yes; then
        eval "long_libname=\"${TCL_VENDOR_PREFIX}${libname}${suffix}\""
      else
        eval "long_libname=\"lib${TCL_VENDOR_PREFIX}${libname}${suffix}\""
      fi
    ;;
    *)
      eval "long_libname=\"lib${TCL_VENDOR_PREFIX}${libname}${suffix}\""
    ;;
  esac

  eval "long_libname=${long_libname}"

  # Trick to replace DBGX with TCL_DBGX
  DBGX='${TCL_DBGX}'
  eval "long_libname=${long_libname}"

  $1=$long_libname
])

#--------------------------------------------------------------------
# TCL_TOOL_SHARED_LIB_LONGNAME
#
#	Return the shared library name in the "long format" understood by
#	the build tools. This might involve prepending a suffix
#	and appending version information to the shared library name.
#
# Arguments:
#	
#	VAR
#	LIBNAME
#	SUFFIX
#	
# Depends on:
#	TCL_DBGX
#	TCL_VENDOR_PREFIX
#
# Example:
#
# TCL_TOOL_SHARED_LIB_LONGNAME(TCL_SHLIB, tcl, $TCL_SHARED_LIB_SUFFIX)
#
# Results:
#	The above example could result in the following.
#
#	TCL_SHLIB=libtcl83.so
#
#	or
#
#	TCL_SHLIB=tcl83.dll
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_SHARED_LIB_LONGNAME, [
  libname=$2
  suffix=$3

  case "${host}" in
    *windows32* | *mingw32* | *cygwin*)
      eval "long_libname=\"${TCL_VENDOR_PREFIX}${libname}${suffix}\""
    ;;
    *)
      eval "long_libname=\"lib${TCL_VENDOR_PREFIX}${libname}${suffix}\""
    ;;
  esac

  eval "long_libname=${long_libname}"

  # Trick to replace DBGX with TCL_DBGX
  DBGX='${TCL_DBGX}'
  eval "long_libname=${long_libname}"

  $1=$long_libname
])

#--------------------------------------------------------------------
# TCL_TOOL_LIB_SHORTNAME
#
#	Return the library name in the "short format" understood by
#	the build tools. This might involve prepending a suffix
#	and appending version information to the library name.
#	The VC++ compiler does not support short library names
#	so we just use the static import lib name in that case.
#
# Arguments:
#	
#	VAR
#	LIBNAME
#	VERSION
#	
# Depends on:
#	TCL_LIB_VERSIONS_OK
#	TCL_DBGX
#	SHARED_BUILD
#	
#
# Example:
#
# TCL_TOOL_LIB_SHORTNAME(TCL_LIB, tcl, 8.3)
#
# Results:
#	The above example could result in the following.
#
#	TCL_LIB=-ltcl83
#
#	or
#
#	TCL_LIB=tcl83.lib
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_LIB_SHORTNAME, [
  libname=$2
  version=$3

  if test "$TCL_LIB_SUFFIX" = "" ; then
    AC_MSG_ERROR([The TCL_LIB_SUFFIX variable is not defined])
  fi

  # If the . character is not allowed in lib name, remove it from version
  if test "${TCL_LIB_VERSIONS_OK}" != "ok"; then
        version=`echo $version | tr -d .`
  fi

  case "${host}" in
    *windows32* | *mingw32*)
      if test "$GCC" != yes; then
        eval "short_libname=\"${TCL_VENDOR_PREFIX}${libname}${version}${TCL_LIB_SUFFIX}\""
      else
        short_libname="-l${TCL_VENDOR_PREFIX}${libname}${version}${TCL_DBGX}"
      fi
    ;;
    *)
      short_libname="-l${TCL_VENDOR_PREFIX}${libname}${version}\${TCL_DBGX}"
    ;;
  esac

  $1=$short_libname
])

#--------------------------------------------------------------------
# TCL_TOOL_LIB_SPEC
#
#	Return the "lib spec format" understood by the build tools.
#
# Arguments:
#	
#	VAR
#	DIR
#	LIBARG
#	
# Depends on:
#	
#
# Example:
#
# TCL_TOOL_LIB_SPEC(SPEC, /usr/lib, -ltcl)
#
# Results:
#	The above example could result in the following.
#
#	SPEC="-L/usr/lib -ltcl83"
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_LIB_SPEC, [
  case "${host}" in
    *windows32* | *mingw32*)
      if test "$GCC" != yes; then
        TCL_TOOL_PATH($1, "$2/$3")
      else
        TCL_TOOL_PATH(dirname, $2)
        $1="-L${dirname} $3"
      fi
    ;;
    *)
      $1="-L$2 $3"
    ;;
  esac
])

#--------------------------------------------------------------------
# TCL_TOOL_LIB_PATH
#
#	Return the "lib path format" understood by the build tools.
#	Typically, this is the fully qualified path name of the library.
#
# Arguments:
#	
#	VAR
#	DIR
#	LIBARG
#	
# Depends on:
#	
#
# Example:
#
# TCL_TOOL_LIB_PATH(TMP_PATH, /usr/lib, libtcl83.a)
#
# Results:
#	The above example could result in the following.
#
#	TMP_PATH="/usr/lib/libtcl83.a"
#
#--------------------------------------------------------------------

AC_DEFUN(TCL_TOOL_LIB_PATH, [
  TCL_TOOL_PATH($1, "$2/$3")
])
