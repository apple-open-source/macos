#! /bin/sh

PATH=/bin:/usr/bin

arch=""
for i in /sbin/arch /bin/arch /usr/sbin/arch /usr/bin/arch; do
    if [ -x "$i" ]; then
        archbin="$i"
    fi
done

if [ -z "$archbin" ]; then
    echo "Unable to locate the 'arch' utility; assuming 'ppc'.";
    arch="ppc";
else
    arch=`(unset DYLD_PRINT_LIBRARIES; "$archbin") 2>/dev/null` || arch=""
fi

if [ -z "$arch" ]; then
    echo "There was an error executing '$archbin'; assuming 'ppc'.";
    arch="ppc";
fi

case "$arch" in
    ppc)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-macos10"
        ;;
    i386)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-macos10"
        ;;
    *)
        echo "Unknown architecture '$arch'; using 'ppc' instead.";
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-macos10"
        ;;
esac

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

exec "$gdb" "$@"
