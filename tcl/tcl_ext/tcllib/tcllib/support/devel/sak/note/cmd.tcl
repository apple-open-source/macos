# -*- tcl -*-
# Implementation of 'note'.

# Available variables
# * argv  - Cmdline arguments
# * base  - Location of sak.tcl = Top directory of Tcllib distribution
# * cbase - Location of all files relevant to this command.
# * sbase - Location of all files supporting the SAK.

package require sak::util
package require sak::note

set raw  0
set log  0
set stem {}
set tclv {}

if {![llength $argv]} {
    sak::note::show
    return
}
if {[llength $argv] < 3} {
    sak::note::usage
}
foreach {m p} $argv break
set notes [lrange $argv 2 end]

sak::note::run $m $p $notes

##
# ###
