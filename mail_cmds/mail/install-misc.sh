set -e -x

ETCDIR="$DSTROOT"/private/etc
MISCDIR="$DSTROOT"/usr/share/misc

install -d -o root -g wheel -m 0755 "$MISCDIR"
install -c -o root -g wheel -m 0644 \
	"$SRCROOT"/mail/misc/mail.help \
	"$SRCROOT"/mail/misc/mail.tildehelp \
	"$MISCDIR"
install -d -o root -g wheel -m 0755 "$ETCDIR"
install -c -o root -g wheel -m 0644 \
	"$SRCROOT"/mail/misc/mail.rc \
	"$ETCDIR"
