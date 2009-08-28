#
# Copyright (C) 1997-2000 Matt Newman <matt@novadigm.com>
#
# $Header: /cvsroot/tls/tls/tests/oldTests/tlsCiphers.tcl,v 1.1 2000/06/06 18:13:21 aborr Exp $
#

set dir [file dirname [info script]]
cd $dir
source tls.tcl

if {[llength $argv] == 0} {
    puts stderr "Usage: ciphers protocol ?verbose?"
    exit 1
}
puts [join [eval tls::ciphers $argv] \n]
exit 0
