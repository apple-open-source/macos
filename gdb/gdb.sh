#! /bin/sh

host_architecture=""
requested_architecture=""
architecture_to_use=""

# classic-inferior-support
oah750_mode=0
oah750_binary=""

PATH=$PATH:/sbin:/bin:/usr/sbin:/usr/bin

host_architecture=`(unset DYLD_PRINT_LIBRARIES; "arch") 2>/dev/null` || host_architecture=""

if [ -z "$host_architecture" ]; then
    echo "There was an error executing 'arch(1)'; assuming 'ppc'.";
    host_architecture="ppc";
fi


case "$1" in
  --help)
    echo "  --oah750           Debug classic applications running under oah750." >&2
    echo "  -arch i386|ppc     Specify a gdb targetting either ppc or i386" >&2
    ;;
  -arch=* | -a=* | --arch=*)
    requested_architecture=`echo "$1" | sed 's,^[^=]*=,,'`
    shift;;
  -arch | -a | --arch)
    shift
    requested_architecture="$1"
    shift;;
  -oah750 | --oah750 | -oah* | --oah*)
    oah750_mode=1
    shift;;
esac

if [ $oah750_mode -eq 1 ]
then
  if [ "$host_architecture" = i386 -a -x /usr/libexec/oah/oah750 ]
  then
    requested_architecture="ppc"
    oah750_binary=/usr/libexec/oah/oah750
  else
    echo ERROR: oah750 not available.  Running in normal debugger mode. >&2
  fi
fi

if [ -n "$requested_architecture" ]
then
  if [ "$requested_architecture" != ppc -a "$requested_architecture" != i386 ]
  then
    echo Unrecognized architecture \'$requested_architecture\', using host arch. >&2
    requested_architecture=""
  fi
fi

if [ -n "$requested_architecture" ]
then
  architecture_to_use="$requested_architecture"
else
  architecture_to_use="$host_architecture"
fi

case "$architecture_to_use" in
    ppc)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
    i386)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-darwin"
        ;;
    *)
        echo "Unknown architecture '$architecture_to_use'; using 'ppc' instead.";
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
esac

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

exec $oah750_binary "$gdb" "$@"
