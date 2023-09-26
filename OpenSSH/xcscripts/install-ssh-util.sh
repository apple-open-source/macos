#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"
xmkdir $(dst /usr/local/bin)
INSTALL_MODE=0755 xinstall $(src ssh-util.rb) $(dst /usr/local/bin/ssh-util)
