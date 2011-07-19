#
# m4 configure macros specific to Img.
#

AC_DEFUN(IMG_SRCPATH, [
#--------------------------------------------------------------------
# Compute an absolute path to the src directory of module '$1' so
# that we are able to find its headers even if they are not installed.
#--------------------------------------------------------------------

case [$]$1_SRC_DIR in
/*)	$1_SRC_PATH=[$]$1_SRC_DIR
	;;
*)	# SRC_DIR relative, splice with BUILD_PATH
	$1_SRC_PATH="`dirname [$]$1_BUILD_STUB_LIB_PATH`/[$]$1_SRC_DIR"
esac

$1_BUILD_PATH="`dirname [$]$1_BUILD_STUB_LIB_PATH`"

if test "[$]{TEA_PLATFORM}" = "windows" ; then
    $1_SRC_PATH="`[$]CYGPATH [$]$1_SRC_PATH`"
    $1_BUILD_PATH="`[$]CYGPATH [$]$1_BUILD_PATH`"
fi

AC_SUBST($1_SRC_PATH)
AC_SUBST($1_BUILD_PATH)
])

#
# Add here whatever m4 macros you want to define for your package
#

AC_DEFUN(TEA_CONFIG_SUBDIR, [
    tea_config_dir="$1"
    tea_config_arguments="$2"
    subdirs="$subdirs $tea_config_dir"
    AC_SUBST(subdirs)

    # Do not complain, so a configure script can configure whichever
    # parts of a large source tree are present.
    if test ! -d $srcdir/$tea_config_dir; then
      continue
    fi

    echo configuring in $tea_config_dir

    case "$srcdir" in
    .) ;;
    *)
      if test -d ./$tea_config_dir || mkdir -p ./$tea_config_dir; then :;
      else
        { echo "configure: error: can not create `pwd`/$tea_config_dir" 1>&2; exit 1; }
      fi
      ;;
    esac

    tea_popdir=`pwd`
    cd $tea_config_dir

      # A "../" for each directory in /$tea_config_dir.
      tea_dots=`echo $tea_config_dir|sed -e 's%^\./%%' -e ['s%[^/]$%&/%'] -e ['s%[^/]*/%../%g']`

    case "$srcdir" in
    .) # No --srcdir option.  We are building in place.
      tea_sub_srcdir=$srcdir ;;
    /*) # Absolute path.
      tea_sub_srcdir=$srcdir/$tea_config_dir ;;
    *) # Relative path.
      tea_sub_srcdir=$tea_dots$srcdir/$tea_config_dir ;;
    esac

    # Check for guested configure; otherwise get Cygnus style configure.
    if test -f $tea_sub_srcdir/configure; then
      tea_sub_configure=$tea_sub_srcdir/configure
    elif test -f $tea_sub_srcdir/configure.in; then
      tea_sub_configure=$tea_configure
    else
      echo "configure: warning: no configuration information is in $tea_config_dir" 1>&2
      tea_sub_configure=
    fi

    # The recursion is here.
    if test -n "$tea_sub_configure"; then

      # Force usage of a cache file.
      if test "X$cache_file" = "X/dev/null" ; then
	cache_file=config.cache
      fi

      # Make the cache file name correct relative to the subdirectory.
      case "$cache_file" in
      /*) tea_sub_cache_file=$cache_file ;;
      *) # Relative path.
        tea_sub_cache_file="$tea_dots$cache_file" ;;
      esac

      echo "running ${CONFIG_SHELL-/bin/sh} $tea_sub_configure $tea_sub_configure_args --cache-file=$tea_sub_cache_file --srcdir=$tea_sub_srcdir $tea_config_arguments"
      # The eval makes quoting arguments work.
      if eval ${CONFIG_SHELL-/bin/sh} $tea_sub_configure $tea_sub_configure_args --cache-file=$tea_sub_cache_file --srcdir=$tea_sub_srcdir $tea_config_arguments
      then :
      else
        { echo "configure: error: $tea_sub_configure failed for $tea_config_dir" 1>&2; exit 1; }
      fi
    fi

    cd $tea_popdir
])


AC_DEFUN(TEA_CONFIG_COLLECT, [
  tea_sub_configure_args=
  tea_prev=
  for tea_arg in $ac_configure_args; do
    if test -n "$tea_prev"; then
      tea_prev=
      continue
    fi
    case "$tea_arg" in
    -cache-file | --cache-file | --cache-fil | --cache-fi \
    | --cache-f | --cache- | --cache | --cach | --cac | --ca | --c)
      tea_prev=cache_file ;;
    -cache-file=* | --cache-file=* | --cache-fil=* | --cache-fi=* \
    | --cache-f=* | --cache-=* | --cache=* | --cach=* | --cac=* | --ca=* | --c=*)
      ;;
    -srcdir | --srcdir | --srcdi | --srcd | --src | --sr)
      tea_prev=srcdir ;;
    -srcdir=* | --srcdir=* | --srcdi=* | --srcd=* | --src=* | --sr=*)
      ;;
    *) tea_sub_configure_args="$tea_sub_configure_args $tea_arg" ;;
    esac
  done
])

