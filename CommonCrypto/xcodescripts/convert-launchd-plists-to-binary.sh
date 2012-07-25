/bin/sh
set -e
mkdir -p "$DSTROOT"/System/Library/LaunchDaemons
for plist in "$SRCROOT"/cc_fips_test/*.plist; do
	plutil -convert binary1 "$plist" -o "$DSTROOT"/System/Library/LaunchDaemons/$(basename "$plist")
done
