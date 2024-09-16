#!/bin/sh

# Shim to build cstream; run configure as needed and make sure we inject our
# desired -arch flags.
scriptdir=$(realpath $(dirname "$0"))
buildroot=${OBJECT_FILE_DIR}

for arch in ${ARCHS}; do
	CFLAGS="${CFLAGS} -arch ${arch}"
done
CPPFLAGS="${CPPFLAGS} -DVERSION=\"4.0.0\""
CPPFLAGS="${CPPFLAGS} -DHAVE_LIBM=1 -DHAVE_STDIO_H=1 -DHAVE_STDLIB_H=1
-DHAVE_STRING_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_STRINGS_H=1
-DHAVE_SYS_STAT_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_UNISTD_H=1 -DHAVE_SYS_TIME_H=1
-DSTDC_HEADERS=1 -DHAVE_SYS_WAIT_H=1 -DHAVE_FCNTL_H=1 -DHAVE_SYS_TIME_H=1
-DHAVE_UNISTD_H=1 -DHAVE_POLL_H=1 -DHAVE_SYS_UIO_H=1 -DHAVE_SYS_SOCKET_H=1
-DHAVE_NETINET_IN_H=1 -DHAVE_ARPA_INET_H=1 -DHAVE_NETDB_H=1
-DHAVE_SYS_STATVFS_H=1 -DTIME_WITH_SYS_TIME=1 -DHAVE_SOCKLEN_T=1
-DHAVE_STRUCT_SIGACTION=1 -DHAVE_STRUCT_ITIMERVAL=1 -DRETSIGTYPE=void
-DHAVE_GETTIMEOFDAY=1 -DHAVE_MKFIFO=1 -DHAVE_STRDUP=1 -DHAVE_GETNAMEINFO=1
-DHAVE_GETADDRINFO=1"

stale()
{

	if [ ! -f "$buildroot"/cstream ]; then
		return 0
	fi

	if [ "$buildroot"/cstream -ot "$scriptdir"/cstream.c ]; then
		return 0
	fi

	return 1
}

build()
{
	mkdir -p "$buildroot"
	cc $CFLAGS $CPPFLAGS -o "$buildroot"/cstream "$scriptdir"/cstream.c
}

case "$1" in
installhdrs)
	# No headers to install, don't bother proxying through to make(1).
	;;
install)
	if stale; then
		build
	fi

	install -d -o "$INSTALL_OWNER" -g "$INSTALL_GROUP" \
	    -m 0755 "$INSTALL_DIR"
	install -o "$INSTALL_OWNER" -g "$INSTALL_GROUP" \
	    -m 0755 "$buildroot"/cstream "$INSTALL_DIR"

	;;
clean)
	rm -rf "$buildroot" >/dev/null 2>&1
	;;
*)
	if stale; then
		build
	fi
	;;
esac
