#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"
/bin/ln -fs ssh $(dst /usr/bin/slogin)
/bin/ln -fs ssh.1 $(dst /usr/share/man/man1/slogin.1)
