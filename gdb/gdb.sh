#! /bin/sh

sys=`(unset DYLD_PRINT_LIBRARIES; /usr/libexec/config.guess) 2>/dev/null` || sys=unknown

if [ "$GDB_ROOT" = "" ]; then
    GDB_ROOT=""
fi

case ${sys} in
    powerpc-apple-rhapsody*)
	gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-rhapsody"
	;;
    *86-apple-rhapsody*)
	gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-rhapsody"
	;;
    powerpc-apple-macos*|powerpc-apple-darwin*)
	gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-macos10"
	;;
    *86-apple-macos*|*86-apple-darwin*)
	gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-macos10"
	;;
    *)
	echo "Unable to start GDB: unknown architecture \"${sys}\""
	exit 1

esac

if [ ! -x ${gdb} ]; then
    echo "Unable to start GDB: cannot find binary in \"${gdb}\""
    exit 1
fi

exec ${gdb} "$@"
