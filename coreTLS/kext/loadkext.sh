#!/bin/sh

# install this script in /var/root/ to use with the xcode project.

cp -R $1 /tmp
kextunload -v /tmp/tlsnke.kext/
kextload -v /tmp/tlsnke.kext/

