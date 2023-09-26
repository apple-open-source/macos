#!/bin/sh
set -e -x

BINDIR="$DSTROOT"/usr/bin
LIBEXECDIR="$DSTROOT"/usr/libexec
MANDIR="$DSTROOT"/usr/share/man
PAMDIR="$DSTROOT"/private/etc/pam.d

ln -f "$BINDIR/hexdump" "$BINDIR/od"
ln -f "$BINDIR/id" "$BINDIR/groups"
ln -f "$BINDIR/id" "$BINDIR/whoami"
if [ $PLATFORM_NAME != 'iphonesimulator' ]; then
	ln -f "$BINDIR/w" "$BINDIR/uptime"
fi
ln -f "$DSTROOT/bin/test" "$DSTROOT/bin/["

install -m 0755 -d "$BINDIR"
install -m 0755 -d "$MANDIR"/man1
install -m 0755 -d "$MANDIR"/man8

install -c -m 0755 "$SRCROOT"/alias/generic.sh "$BINDIR"/alias
install -c -m 0644 "$SRCROOT"/alias/builtin.1 "$MANDIR"/man1

set +x
for builtin in `cat "$SRCROOT/xcodescripts/builtins.txt"`; do
	echo ... linking $builtin
	ln -f "$BINDIR"/alias "$BINDIR/$builtin"
done

for manpage in `cat "$SRCROOT/xcodescripts/builtins-manpages.txt"`; do
	echo ... linking $manpage
	echo ".so man1/builtin.1" > "$MANDIR/man1/$manpage"
done
set -x

# Skip locate and su targets for iOS
if [ "$TARGET_NAME" = "All_iOS" ]; then
	exit 0
fi
	
install -m 0755 -d "$LIBEXECDIR"
install -c -m 0755 "$SRCROOT"/locate/locate/updatedb.sh \
	"$LIBEXECDIR"/locate.updatedb
install -c -m 0644 "$SRCROOT"/locate/locate/locate.updatedb.8 \
	"$MANDIR"/man8
install -c -m 0755 "$SRCROOT"/locate/locate/concatdb.sh \
	"$LIBEXECDIR"/locate.concatdb
echo ".so man8/locate.updatedb.8" > "$MANDIR"/man8/locate.concatdb.8
install -c -m 0755 "$SRCROOT"/locate/locate/mklocatedb.sh \
	"$LIBEXECDIR"/locate.mklocatedb
echo ".so man8/locate.updatedb.8" > "$MANDIR"/man8/locate.mklocatedb.8

install -m 0755 -d "$PAMDIR"
install -c -m 0644 "$SRCROOT"/su/su.pam "$PAMDIR"/su
