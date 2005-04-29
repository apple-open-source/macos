#!/bin/sh

SECURITY=${SECURITY:=security}
EMAIL=${EMAIL:=$USER@apple.com}
SSLVIEW=${SSLVIEW:=sslViewer}
SERVER=${SERVER:=hurljo3.apple.com}
HOME=/tmp/test$$

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

echo "CONTENT: The secret and possibly signed content." > content.txt

echo "Connecting to SSL Test server " $SERVER
$SSLVIEW $SERVER r c P=4443 V 3 a

# arch-tag: 51571215-09B6-11D9-8D4F-000A95C4302E

