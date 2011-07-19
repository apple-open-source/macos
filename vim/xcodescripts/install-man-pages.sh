#!/bin/sh
set -e
set -x

MANDIR="$DSTROOT"/usr/share/man/man1

install -o "$ALTERNATE_OWNER" \
	-g "$ALTERNATE_GROUP" \
	-m "$ALTERNATE_MODE",a+x \
	-d "$MANDIR"

for X in `find "$SRCROOT"/runtime/doc -name \*.1 -a \! -name \*-\*.1` ; do
	install -o "$ALTERNATE_OWNER" \
		-g "$ALTERNATE_GROUP" \
		-m "$ALTERNATE_MODE" \
		"$X" \
		"$MANDIR"
done

ln "$MANDIR"/vim.1 "$MANDIR"/ex.1
ln "$MANDIR"/vim.1 "$MANDIR"/vi.1
ln "$MANDIR"/vim.1 "$MANDIR"/view.1
ln "$MANDIR"/vim.1 "$MANDIR"/rvim.1
ln "$MANDIR"/vim.1 "$MANDIR"/rview.1
