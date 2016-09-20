set -ex


# Note using builder version of file until it's easy to be self-hosting <rdar://problem/7140945>
FILE="$DSTROOT"/usr/bin/file
if "$FILE" --version 2>&1 | grep -q "Bad CPU type in executable"; then
	sh "$SRCROOT"/xcodescripts/build_magichost.sh "${BUILT_PRODUCTS_DIR}"
	FILE="$BUILT_PRODUCTS_DIR"/magic_host-dst/magic_host
fi

cd "$TEMP_DIR" && "$FILE" -C -m "$SRCROOT"/file/magic/Magdir

install -d -m 0755 "$DSTROOT"/usr/share/file
install -m 0644 "$TEMP_DIR"/Magdir.mgc "$DSTROOT"/usr/share/file/magic.mgc

install -d -m 0755 "$DSTROOT"/usr/share/file/magic
install -m 0644 "$SRCROOT"/file/magic/Magdir/* "$DSTROOT"/usr/share/file/magic
