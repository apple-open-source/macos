# all-bench.tcl --
#
# This file contains the top-level script to run the tDOM bench mark
# suite. 
# 
# Copyright (c) 2007 by Rolf Ade (rolf@pointsman.de).
#
# $Id: all-bench.tcl,v 1.1 2007/09/27 23:17:02 rolf Exp $
#
# The script knows the options:
#
# -interppath <dir-path>
# Adds a path to the list of pathes, which are searched for tclshs
# with the pattern given by the -pattern option. Every time, this
# option is given, the option value is added to the search list. If
# not given, it is set to directory the tclsh, running the script,
# lives in.
#
# -norm <number>
# See man bench, ::bench::norm
#
# -pattern <pattern>
# Specifies the search pattern for tclshs in the search
# directories. Defaults to tclsh*
#
# -file <pattern>
# Specifies the search pattern for bench files in the same directory
# as the all-bench.tcl. The default is *.bench.
#
# Every known option to ::bench::run (as of 0.3.1) are passed throu to
# that command. See man bench for the details.
# 
# Example:
# tclsh all-bench.tcl \
#     -interppath /usr/local/lib \
#     -pkgdir ~/tdom/tdom-0.8.2 \
#     -pkgdir ~/tdom/tdom-0.8.2-mod
#
# You can measure the same tDOM version against various tclshs, or
# different tDOM versions with the same tclsh, or different tDOM
# versions with differenct tclshs, all side by side.
#
# Don't run this script with a tcldomsh, until you know, what you're
# doing.

# bench 0.3.1 added the -pkgdir flag; we need at least that version,
# if we want to compare more than one tDOM versions side by side.
package require bench 0.3.1
package require bench::out::text


# Defaults / Initialization
set interpPattern tclsh*
set benchFilePattern *.bench
set interpPaths [list]
set benchFlags [list]
# Empty string means: no normalization
set norm ""

foreach {arg argValue} $argv {
    switch -- $arg {
        "-interppath" {
            lappend interpPaths $argValue
        }
        "-norm" {
            if {![string is integer -strict $argValue] || $argValue < 1} {
                puts stderr "The option -norm expects a postiv integer as\
                             value."
                exit 1
            }
            set norm $argValue
        }
        "-pattern" {
            set interpPattern $argValue
        }
        "-file" {
            set benchFilePattern $argValue
        }
        default {
            switch -- $arg {
                "-errors" -
                "-threads" -
                "-match" -
                "-rmatch" -
                "-iters" -
                "-pkgdir" {
                    lappend benchFlags $arg $argValue
                }
                default {
                    puts stderr "Unknown option '$arg'"
                    exit 1
                }
            }
        }
    }
}


if {[llength $interpPaths] == 0} {
    lappend interpPaths [file dirname [info nameofexecutable]]
    puts $interpPaths
}

puts [bench::locate $interpPattern $interpPaths]

set interps [bench::versions [bench::locate $interpPattern $interpPaths]]

if {![llength $interps]} {
    puts stderr "No interpreters found"
    exit 1
}

if {[llength $benchFlags]} {
    set cmd [linsert $benchFlags 0 bench::run]
} else {
    set cmd [list bench::run]
}

set benchfiles [glob -nocomplain [file join [file dir [info script]] \
                                      $benchFilePattern]]
if {![llength $benchfiles]} {
    puts stderr "No benchmark files found."
    exit 1
}

set run $cmd
lappend run $interps $benchfiles
set results [eval $run]
if {$norm ne ""} {
    set results [::bench::norm $results $norm]
}
puts [::bench::out::text $results]
