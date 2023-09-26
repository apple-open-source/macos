#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"
/bin/ln -s ssh $(dst /usr/bin/slogin)
/bin/ln -s ssh.1 $(dst /usr/share/man/man1/slogin.1)
