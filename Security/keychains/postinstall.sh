#!/bin/sh

# Create keychains if not there already
if [ ! -f "$targetdisk/System/Library/Keychains/X509Anchors" ]; then
	"$targetdisk/usr/bin/security" create-keychain -p X509Anchors "$targetdisk/System/Library/Keychains/X509Anchors"
fi
if [ ! -f "$targetdisk/System/Library/Keychains/X509Certificates" ]; then
	"$targetdisk/usr/bin/security" create-keychain -p X509Certificates "$targetdisk/System/Library/Keychains/X509Certificates"
fi

# Add all anchors
cd "$targetdisk/System/Library/Keychains/Anchors/"
"$targetdisk/usr/bin/security" add-certificate -k "$targetdisk/System/Library/Keychains/X509Anchors" *

# Add all intermediates
cd "$targetdisk/System/Library/Keychains/Certificates/"
"$targetdisk/usr/bin/security" add-certificate -k "$targetdisk/System/Library/Keychains/X509Certificates" *

# we might want to delete the raw certificate files, in the interest of cruft cleanup
#rm -rf "$targetdisk/System/Library/Keychains/Anchors"
#rm -rf "$targetdisk/System/Library/Keychains/Certificates"
