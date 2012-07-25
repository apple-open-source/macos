set -ex

install -d -m 0755 "$DSTROOT"/usr/local/OpenSourceLicenses \
	"$DSTROOT"/usr/local/OpenSourceVersions

install -m 0644 "$SRCROOT"/file/COPYING \
	"$DSTROOT"/usr/local/OpenSourceLicenses/file.txt

install -m 0644 "$SRCROOT"/file.plist \
	"$DSTROOT"/usr/local/OpenSourceVersions/file.plist
