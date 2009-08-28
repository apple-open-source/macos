#! /bin/sh

host_arch=""
requested_arch="UNSET"
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

host_arch=`/usr/bin/arch 2>/dev/null` || host_arch=""

if [ -z "$host_arch" ]; then
    echo "There was an error executing 'arch(1)'; assuming 'i386'.";
    host_arch="i386";
fi

# Not sure if this helps anything in particular - gdb should pick the
# x86_64 arch by default when available and the hardware supports it.
# And it might cause issues with some of our older branches so I'll
# leave it commented out for the moment.
#
#if [ $host_arch = i386 ]
#then
#  x86_64_p=`sysctl -n hw.optional.x86_64 2>/dev/null`
#  if [ -n "$x86_64_p" -a "$x86_64_p" = "1" ]
#  then
#    host_arch=x86_64
#  fi
#fi

case "$1" in
 --help)
    echo "  --translate        Debug applications running under translation."
    echo "  -arch i386|armv6|x86_64|ppc     Specify a gdb targetting a specific architecture"
    ;;
  -arch=* | -a=* | --arch=*)
    requested_arch=`echo "$1" | sed 's,^[^=]*=,,'`
    shift;;
  -arch | -a | --arch)
    shift
    requested_arch="$1"
    shift;;
  -translate | --translate | -oah* | --oah*)
    translate_mode=1
    shift;;
esac

if [ -z "$requested_arch" ]
then
  echo ERROR: No architecture specified with -arch argument. >&2
  exit 1
fi
[ "$requested_arch" = "UNSET" ] && requested_arch=""

if [ $translate_mode -eq 1 ]
then
  translate=""
  if [ "$host_arch" = i386 -o "$host_arch" = x86_64 ]
  then
    translate=`sysctl -n kern.exec.archhandler.powerpc`
  fi
  [ -z "$translate" -o ! -x "$translate" ] && translate=/usr/libexec/oah/translate
  if [ "$host_arch" = i386 -a -x $translate ]
  then
    requested_arch="ppc"
    translate_binary="$translate -execOAH"
  else
    if [ "$host_arch" = x86_64 -a -x $translate ]
    then
      requested_arch="ppc"
      translate_binary="$translate -execOAH"
    else
      echo ERROR: translate not available.  Running in normal debugger mode. >&2
    fi
  fi
fi

if [ -n "$requested_arch" ]
then
  case $requested_arch in
    ppc* | i386 | x86_64 | arm*)
     ;;
    *)
      echo Unrecognized architecture \'$requested_arch\', using host arch. >&2
      requested_arch=""
      ;;
  esac
fi

if [ -n "$requested_arch" ]
then
  architecture_to_use="$requested_arch"
else
  # No architecture was specified. We will try to find the executable
  # or a core file in the list of arguments, and launch the correct
  # gdb for the job. If there are multiple architectures in the executable,
  # we will search for the architecture that matches the host architecture.
  # If all this searching doesn't produce a match, we will use a gdb that
  # matches the host architecture by default.
  best_arch=
  exec_file=
  core_file=
  for arg in "$@"
  do
    case "$arg" in
      -*)
        # Skip all option arguments
        ;;
      *)
        # Call file to determine the file type of the argument
        file_result=`file "$arg"`;
        case "$file_result" in
          *\ Mach-O\ core\ *|*\ Mach-O\ 64-bit\ core\ *)
            core_file=$arg
            ;;
          *\ Mach-O\ *)
            exec_file=$arg
            ;;
          *)
            if [ -x "$arg" ]; then
              exec_file="$arg"
            fi
            ;;
        esac
        ;;
    esac
  done

  # Get a list of possible architectures in FILE_ARCHS.
  # If we have a core file, we must use it to determine the architecture,
  # else we use the architectures in the executable file.
  file_archs=
  if [ -n "$core_file" ]
  then
    core_file_tmp=`file "$core_file" 2>/dev/null | tail -1`
  fi
  if [ -n "$core_file" -a -n "$core_file_tmp" ]
  then
    # file(1) has a weird way of identifying x86_64 core files; they have
    # a magic of MH_MAGIC_64 but a cputype of CPU_TYPE_I386.  Probably a bug.
    if echo "$core_file_tmp" | grep 'Mach-O 64-bit core i386' >/dev/null
    then
      file_archs=x86_64
    else
      file_archs=`echo "$core_file_tmp" | awk '{print $NF}'`
    fi
  else
    if [ -n "$exec_file" ]
    then
      file_archs=`file "$exec_file" | grep -v universal | awk '{ print $NF }'`
    fi
  fi

  # Iterate through the architectures and try and find the best match.
  for file_arch in $file_archs 
  do
    # If we don't have any best architecture set yet, use this in case
    # none of them match the host architecture.
    if [ -z "$best_arch" ]; then
      best_arch="$file_arch"
    fi

    # See if the file architecture matches the host, and if so set the
    # best architecture to that.
    if [ "$file_arch" = "$host_arch" ]; then
      best_arch="$file_arch"
    fi
  done

  case "$best_arch" in
    ppc* | i386 | x86_64 | arm*)
      # We found a plausible architecture and we will use it
      architecture_to_use="$best_arch"
      ;;
    *)
      # We did not find a plausible architecture, use the host architecture
      architecture_to_use="$host_arch"
      ;;
  esac
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
  if [ "$GDB_ROOT" = "/" ]
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
  arm*)
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-arm-apple-darwin"
      case "$architecture_to_use" in
        armv6) 
          osabiopts="--osabi DarwinV6" 
          ;;
        *)
          # Make the REQUESTED_ARCHITECTURE the empty string so
          # we can let gdb auto-detect the cpu type and subtype
          requested_arch=""
          ;;
      esac
      ;;
  *)
    echo "Unknown architecture '$architecture_to_use'; using 'i386' instead.";
    gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-darwin"
    ;;
esac

# If we have a core file and the user didn't specify an architecture, we need
# to set the REQUESTED_ARCH to the architecture to use in case we have a 
# universal executable with a core file (which is always skinny). This is a
# bug in gdb currently that hasn't been fixed. If gdb ever does fix its 
# ability to grab the correct slice from an executable given a core file, 
# then we can take the next 3 lines out.
if [ -z "$requested_arch" -a -n "$core_file" ]; then
  requested_arch=$architecture_to_use;      
fi

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

if [ -n "$requested_arch" -a $translate_mode -eq 0 ]
then
  exec $translate_binary "$gdb" --arch "$requested_arch" "$@"
else
  exec $translate_binary "$gdb" $osabiopts "$@"
fi
