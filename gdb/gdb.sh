#! /bin/sh

host_architecture=""
requested_architecture=""
architecture_to_use=""

# classic-inferior-support
translate_mode=0
translate_binary=""

PATH=$PATH:/sbin:/bin:/usr/sbin:/usr/bin

# gdb is setgid procmod and dyld will truncate any DYLD_FRAMEWORK_PATH etc
# settings on exec.  The user is really trying to set these things
# in their process, not gdb.  So we smuggle it over the setgid border in
# GDB_DYLD_* where it'll be laundered inside gdb before invoking the inferior.

unset GDB_DYLD_FRAMEWORK_PATH
unset GDB_DYLD_FALLBACK_FRAMEWORK_PATH
unset GDB_DYLD_LIBRARY_PATH
unset GDB_DYLD_FALLBACK_LIBRARY_PATH
unset GDB_DYLD_ROOT_PATH
unset GDB_DYLD_PATHS_ROOT
unset GDB_DYLD_IMAGE_SUFFIX
unset GDB_DYLD_INSERT_LIBRARIES
[ -n "$DYLD_FRAMEWORK_PATH" ] && GDB_DYLD_FRAMEWORK_PATH="$DYLD_FRAMEWORK_PATH"
[ -n "$DYLD_FALLBACK_FRAMEWORK_PATH" ] && GDB_DYLD_FALLBACK_FRAMEWORK_PATH="$DYLD_FALLBACK_FRAMEWORK_PATH"
[ -n "$DYLD_LIBRARY_PATH" ] && GDB_DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH"
[ -n "$DYLD_FALLBACK_LIBRARY_PATH" ] && GDB_DYLD_FALLBACK_LIBRARY_PATH="$DYLD_FALLBACK_LIBRARY_PATH"
[ -n "$DYLD_ROOT_PATH" ] && GDB_DYLD_ROOT_PATH="$DYLD_ROOT_PATH"
[ -n "$DYLD_PATHS_ROOT" ] && GDB_DYLD_PATHS_ROOT="$DYLD_PATHS_ROOT"
[ -n "$DYLD_IMAGE_SUFFIX" ] && GDB_DYLD_IMAGE_SUFFIX="$DYLD_IMAGE_SUFFIX"
[ -n "$DYLD_INSERT_LIBRARIES" ] && GDB_DYLD_INSERT_LIBRARIES="$DYLD_INSERT_LIBRARIES"
export GDB_DYLD_FRAMEWORK_PATH
export GDB_DYLD_FALLBACK_FRAMEWORK_PATH
export GDB_DYLD_LIBRARY_PATH
export GDB_DYLD_FALLBACK_LIBRARY_PATH
export GDB_DYLD_ROOT_PATH
export GDB_DYLD_PATHS_ROOT
export GDB_DYLD_IMAGE_SUFFIX
export GDB_DYLD_INSERT_LIBRARIES

# dyld will warn if any of these are set and the user invokes a setgid program
# like gdb.
unset DYLD_FRAMEWORK_PATH
unset DYLD_FALLBACK_FRAMEWORK_PATH
unset DYLD_LIBRARY_PATH
unset DYLD_FALLBACK_LIBRARY_PATH
unset DYLD_ROOT_PATH
unset DYLD_PATHS_ROOT
unset DYLD_IMAGE_SUFFIX
unset DYLD_INSERT_LIBRARIES

host_architecture=`/usr/bin/arch 2>/dev/null` || host_architecture=""

if [ -z "$host_architecture" ]; then
    echo "There was an error executing 'arch(1)'; assuming 'ppc'.";
    host_architecture="ppc";
fi


case "$1" in
  --help)
    echo "  --translate        Debug applications running under translate." >&2
    echo "  -arch i386|ppc     Specify a gdb targetting either ppc or i386" >&2
    ;;
  -arch=* | -a=* | --arch=*)
    requested_architecture=`echo "$1" | sed 's,^[^=]*=,,'`
    shift;;
  -arch | -a | --arch)
    shift
    requested_architecture="$1"
    shift;;
  -translate | --translate | -oah* | --oah*)
    translate_mode=1
    shift;;
esac

if [ $translate_mode -eq 1 ]
then
  if [ "$host_architecture" = i386 -a -x /usr/libexec/oah/translate ]
  then
    requested_architecture="ppc"
    translate_binary="/usr/libexec/oah/translate -execOAH"
  else
    echo ERROR: translate not available.  Running in normal debugger mode. >&2
  fi
fi

if [ -n "$requested_architecture" ]
then
  case $requested_architecture in
    ppc* | i386 | x86_64)
     ;;
    *)
      echo Unrecognized architecture \'$requested_architecture\', using host arch. >&2
      requested_architecture=""
      ;;
  esac
fi

if [ -n "$requested_architecture" ]
then
  architecture_to_use="$requested_architecture"
else
  architecture_to_use="$host_architecture"
fi

# If GDB_ROOT is not set, then figure it out
# from $0.  We need this for gdb's that are
# not installed in /usr/bin.

GDB_ROOT_SET=${GDB_ROOT:+set}
if [ "$GDB_ROOT_SET" != "set" ]
then
  gdb_bin="$0"
  if [ -L "$gdb_bin" ]
  then
    gdb_bin=`readlink "$gdb_bin"`
  fi
  gdb_bin_dirname=`dirname "$gdb_bin"`
  GDB_ROOT=`cd "$gdb_bin_dirname"/../.. ; pwd`
  if [ $"GDB_ROOT" = "/" ]
      then
        GDB_ROOT=
  fi
fi

case "$architecture_to_use" in
    ppc*)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
    i386 | x86_64)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-darwin"
        ;;
    *)
        echo "Unknown architecture '$architecture_to_use'; using 'ppc' instead.";
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
esac

# When running under CodeWarrior we want to invoke a gdb binary
# specifically intended/qualified for its use.

parent=`ps -Awwwo pid,command | 
          awk -v ppid=$PPID '{if ($1 == ppid ) {print}}' |
          sed -e 's,^ *,,' -e 's,[^ ]* *,,'`
if [ -n "$parent" ]
then
  case "$parent" in
    *CodeWarrior*)
      if [ -x "${GDB_ROOT}/usr/libexec/gdb/gdb-for-codewarrior" ]
      then
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-for-codewarrior"
      fi
    ;;
  esac
fi

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

if [ -n "$requested_architecture" -a $translate_mode -eq 0 ]
then
  exec $translate_binary "$gdb" --arch "$requested_architecture" "$@"
else
  exec $translate_binary "$gdb" "$@"
fi
