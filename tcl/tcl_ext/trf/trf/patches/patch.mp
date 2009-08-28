#!/usr/local/bin/tclsh
# -*- tcl -*-
# Generate a manpage for Tcl_ReplaceChannel from a pseudo header!

set ipath [file join [file dirname [file dirname [info library]]] include]
set src   [file join patches rc.h]

puts "c2man -U__GNUC__ -I$ipath -o../doc -Tn $src 2>/dev/null >/dev/null"

exec c2man -U__GNUC__ -I$ipath -o../doc -Tn $src 2>/dev/null >/dev/null
