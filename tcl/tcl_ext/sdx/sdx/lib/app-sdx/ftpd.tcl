#!/bin/sh
#
# ftpd.tcl -- Worlds Smallest FTPD?
#
# Copyright (c) 1999 Matt Newman, Jean-Claude Wippler and Equi4 Software.
# \
exec tclkitsh "`cygpath -w "$0"`" ${1+"$@"}

package require ftpd

proc bgerror msg { tclLog ${::errorInfo} }

ftpd::server $argv

tclLog "Accepting connections on ftp://${ftpd::ipaddr}:${ftpd::port}/"
tclLog "FTP Root: ${ftpd::root}"

if {![info exists tcl_service]} {
    vwait foreever
}
