#!/bin/sh

# usage: point LOCAL_BUILD_DIR to your build folder, insert a card
# and run this script

echo $PATH | fgrep -q "${LOCAL_BUILD_DIR}:" || PATH=${LOCAL_BUILD_DIR}:$PATH
SECURITY=`which security`
HOME=/tmp/test$$
export HOME

mkdir $HOME
cd $HOME
mkdir Library
mkdir Library/Preferences
mkdir Library/Keychains

echo Creating a login.keychain
$SECURITY create -p login login.keychain
echo "listing keychains"
$SECURITY list-keychains
echo "listing default keychain"
$SECURITY default-keychain

echo "Looking for the email address of the first certificate on the card"
if [ "x$EMAIL" == "x" ]; then
	EMAIL=`$SECURITY find-certificate | awk -F = '/\"alis\"<blob>/ { addr=$2; gsub(/\"/, "", addr); print addr }'`
	if [ "x$EMAIL" == "x" ]; then
		echo "No certificate with an email address found."
		exit 1
	fi
fi
echo "Email addres found: <$EMAIL>"

echo "CONTENT: The secret and possibly signed content." > content.txt

echo "Creating a signed cms message."
$SECURITY cms -S -N "$EMAIL" -i content.txt -o signed.cms
echo "Verifying the signed cms message."
$SECURITY cms -D -i signed.cms -h0

echo "Creating an encrypted cms message."
$SECURITY cms -E -r "$EMAIL" -i content.txt -o encrypted.cms
echo "Decrypting the message."
$SECURITY cms -D -i encrypted.cms

#echo "Exporting the identity to pkcs12."
#$SECURITY export -f pkcs12 -t identities -p -P testcms -o identity.p12

# arch-tag: D00EE88A-08E5-11D9-B1C3-000A9595DEEE
