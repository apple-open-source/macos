set -ex

COMMAND="sed -e s@__CSECTION__@1@g -e s@__FSECTION__@5@g -e s@__VERSION__@5.04@g -e s@__MAGIC__@/usr/share/file/magic@g"

case "$TARGET_NAME" in
file)
	install -d -m 0755 "$DSTROOT"/usr/share/man/man1 "$DSTROOT"/usr/share/man/man5
	${COMMAND} "$SRCROOT"/file/doc/file.man > "$DSTROOT"/usr/share/man/man1/file.1
	${COMMAND} "$SRCROOT"/file/doc/magic.man > "$DSTROOT"/usr/share/man/man5/magic.5
	;;
libmagic)
	install -d -m 0755 "$DSTROOT"/usr/local/share/man/man3
	${COMMAND} "$SRCROOT"/file/doc/libmagic.man > "$DSTROOT"/usr/local/share/man/man3/libmagic.3
	;;
*)
	exit 1
	;;
esac
