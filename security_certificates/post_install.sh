#!/bin/sh

TARGETDISK=${3}

# Create keychains if not there already
if [ ! -f "$TARGETDISK/System/Library/Keychains/X509Anchors" ]; then
        "$TARGETDISK/usr/bin/security" create-keychain -p X509Anchors "$TARGETDISK/System/Library/Keychains/X509Anchors"
fi

if [ ! -f "$TARGETDISK/System/Library/Keychains/X509Certificates" ]; then
        "$TARGETDISK/usr/bin/security" create-keychain -p X509Certificates "$TARGETDISK/System/Library/Keychains/X509Certificates"
fi

# Add all anchors
if [ -d "$TARGETDISK/System/Library/Keychains/Anchors" ]; then
        cd "$TARGETDISK/System/Library/Keychains/Anchors"
        "$TARGETDISK/usr/bin/security" add-certificate -k "$TARGETDISK/System/Library/Keychains/X509Anchors" *
	cd /
        /bin/rm -rf "$TARGETDISK/System/Library/Keychains/Anchors"
fi

# Add all intermediates
if [ -d "$TARGETDISK/System/Library/Keychains/Certificates" ]; then
        cd "$TARGETDISK/System/Library/Keychains/Certificates"
        "$TARGETDISK/usr/bin/security" add-certificate -k "$TARGETDISK/System/Library/Keychains/X509Certificates" *
	cd /
        /bin/rm -rf "$TARGETDISK/System/Library/Keychains/Certificates"
fi
