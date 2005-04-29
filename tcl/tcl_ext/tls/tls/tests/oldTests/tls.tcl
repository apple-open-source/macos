#
# Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
#
# $Header: /cvsroot/tls/tls/tests/oldTests/tls.tcl,v 1.2 2000/06/06 18:24:33 aborr Exp $
#
set dir [file dirname [info script]]
regsub {\.} [info tclversion] {} vshort
if {$tcl_platform(platform) == "windows"} {
    if {[info exists tcl_platform(debug)]} {
	load $dir/../win/Debug$vshort/tls.dll
    } else {
	load $dir/../win/Release$vshort/tls.dll
    }
} else {
    load [glob $dir/../unix/libtls*]
}

proc bgerror {err} {
    global errorInfo
    puts stderr "BG Error: $errorInfo"
}

source $dir/../tls.tcl
set tls::debug 2

