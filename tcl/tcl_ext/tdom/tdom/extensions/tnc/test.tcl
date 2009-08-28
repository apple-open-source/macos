
if {[catch {package require tnc 0.3} errMsg]} {
    if {[catch {load ./libtnc0.3.0.so}]} {
        puts $errMsg
        exit 1
    }
}
if {[lsearch [namespace children] ::tDOM] == -1} {
    # tcldomsh without the script library. Source the lib.
    source [file join [file dir [info script]] ../../lib tdom.tcl]
}

if {[llength $argv] != 1} {
    puts stderr "usage: $argv0 <xml-file>"
    exit 1
}

set parser [expat \
        -baseurl [tDOM::baseURL $argv]                          \
        -externalentitycommand tDOM::extRefHandler              \
        -paramentityparsing notstandalone]

tnc $parser enable

$parser parsefile $argv

